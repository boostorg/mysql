//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_CONNECTION_POOL_CONNECTION_NODE_HPP
#define BOOST_MYSQL_DETAIL_CONNECTION_POOL_CONNECTION_NODE_HPP

#include <boost/mysql/any_address.hpp>
#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/pool_params.hpp>
#include <boost/mysql/ssl_mode.hpp>

#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/connection_pool/iddle_connection_list.hpp>
#include <boost/mysql/detail/connection_pool/run_with_timeout.hpp>
#include <boost/mysql/detail/connection_pool/task_joiner.hpp>

#include <boost/asio/async_result.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/experimental/channel_error.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/optional/optional.hpp>
#include <boost/throw_exception.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>

namespace boost {
namespace mysql {
namespace detail {

class connection_pool_impl;

// Same as pool_params, but structured in a way that is more helpful for the impl
struct internal_pool_params
{
    connect_params connect_config;
    optional<asio::ssl::context> ssl_ctx;
    std::size_t initial_read_buffer_size;
    std::size_t initial_size;
    std::size_t max_size;
    std::chrono::steady_clock::duration connect_timeout;
    std::chrono::steady_clock::duration ping_timeout;
    std::chrono::steady_clock::duration retry_interval;
    std::chrono::steady_clock::duration ping_interval;

    any_connection_params make_ctor_params() noexcept
    {
        return {
            ssl_ctx.has_value() ? &ssl_ctx.value() : nullptr,
            initial_read_buffer_size,
        };
    }
};

inline void check_validity(const pool_params& params) noexcept
{
    const char* msg = nullptr;
    if (params.max_size == 0)
        msg = "pool_params::max_size must be greater than zero";
    else if (params.max_size < params.initial_size)
        msg = "pool_params::max_size must be greater than pool_params::initial_size";
    else if (params.connect_timeout.count() < 0)
        msg = "pool_params::connect_timeout can't be negative";
    else if (params.retry_interval.count() <= 0)
        msg = "pool_params::retry_interval must be greater than zero";
    else if (params.ping_interval.count() < 0)
        msg = "pool_params::ping_interval can't be negative";
    else if (params.ping_timeout.count() < 0)
        msg = "pool_params::ping_timeout can't be negative";

    if (msg != nullptr)
    {
        BOOST_THROW_EXCEPTION(std::invalid_argument(msg));
    }
}

inline internal_pool_params make_internal_pool_params(pool_params&& params)
{
    check_validity(params);

    optional<asio::ssl::context> ssl_ctx{std::move(params.ssl_ctx)};
    if (!ssl_ctx.has_value() && params.ssl != ssl_mode::disable &&
        params.server_address.type() == address_type::host_and_port)
    {
        ssl_ctx.emplace(asio::ssl::context::tlsv12_client);
    }

    return {
        {
         std::move(params.server_address),
         std::move(params.username),
         std::move(params.password),
         std::move(params.database),
         std::uint16_t(0), // use the server's default collation
            params.ssl,
         params.multi_queries,
         },
        std::move(ssl_ctx),
        params.initial_read_buffer_size,
        params.initial_size,
        params.max_size,
        params.connect_timeout,
        params.ping_timeout,
        params.retry_interval,
        params.ping_interval,
    };
}

// State shared between connection tasks
struct conn_shared_state
{
    wait_group wait_gp;
    iddle_connection_list iddle_list;
    std::size_t num_pending_connections{0};

    conn_shared_state(boost::asio::any_io_executor ex) : wait_gp(ex), iddle_list(std::move(ex)) {}
};

// Status enums
enum class connection_status
{
    pending_connect,
    pending_reset,
    pending_ping,
    iddle,
    in_use,
};

inline bool is_pending(connection_status status) noexcept
{
    return status != connection_status::iddle && status != connection_status::in_use;
}

enum class next_connection_action
{
    invalid,
    connect,
    sleep_connect_failed,
    iddle_wait,
    reset,
    ping,
};

enum class collection_state
{
    none,
    needs_collect,
    needs_collect_with_reset
};

class sansio_connection_node
{
    asio::coroutine coro_;
    connection_status status_{connection_status::pending_connect};

public:
    BOOST_MYSQL_DECL
    next_connection_action resume(error_code ec, collection_state col_st)
    {
        BOOST_ASIO_CORO_REENTER(coro_)
        {
            while (true)
            {
                if (status_ == connection_status::pending_connect)
                {
                    // Try to connect
                    BOOST_ASIO_CORO_YIELD return next_connection_action::connect;
                    if (ec)
                    {
                        // Sleep
                        BOOST_ASIO_CORO_YIELD return next_connection_action::sleep_connect_failed;

                        // We're still pending connect, just retry.
                    }
                    else
                    {
                        // We're iddle
                        status_ = connection_status::iddle;
                    }
                }
                else if (status_ == connection_status::iddle || status_ == connection_status::in_use)
                {
                    // Iddle wait. Note that, if a connection is taken, status_ will be
                    // changed externally, not by this coroutine. This saves rescheduling.
                    BOOST_ASIO_CORO_YIELD return next_connection_action::iddle_wait;
                    if (col_st != collection_state::none)
                    {
                        // The user has notified us to collect the connection.
                        // This happens after they return the connection to the pool.
                        // Update status and continue
                        status_ = col_st == collection_state::needs_collect
                                      ? connection_status::iddle
                                      : connection_status::pending_reset;
                    }
                    else if (status_ == connection_status::iddle)
                    {
                        // The wait finished with no interruptions, and the connection
                        // is still iddle. Time to ping.
                        status_ = connection_status::pending_ping;
                    }

                    // Otherwise (status is in_use and there's no collection request),
                    // the user is still using the connection (it's taking long, but can happen).
                    // Iddle wait again until they return the connection.
                }
                else if (status_ == connection_status::pending_ping || status_ == connection_status::pending_reset)
                {
                    // Do ping or reset
                    BOOST_ASIO_CORO_YIELD return status_ == connection_status::pending_ping
                        ? next_connection_action::ping
                        : next_connection_action::reset;

                    // Check result
                    if (ec)
                    {
                        // The operation had an error but weren't cancelled. Reconnect
                        status_ = connection_status::pending_connect;
                    }
                    else
                    {
                        // The operation succeeded. We're iddle again.
                        status_ = connection_status::iddle;
                    }
                }
                else
                {
                    BOOST_ASSERT(false);
                }
            }
        }

        return next_connection_action::invalid;
    }

    void mark_as_in_use() noexcept
    {
        BOOST_ASSERT(status_ == connection_status::iddle);
        status_ = connection_status::in_use;
    }

    connection_status status() const noexcept { return status_; }
};

class connection_node : public hook_type
{
    // Not thread-safe, should only be manipulated within the connection pool strand context
    const internal_pool_params* params_;
    sansio_connection_node sansio_impl_;
    any_connection conn_;
    boost::asio::steady_timer timer_;
    conn_shared_state* shared_st_;
    diagnostics diag_;
    bool cancelled_{false};

    // Thread-safe, may be called from any thread
    std::atomic<collection_state> collection_state_{collection_state::none};
    asio::experimental::concurrent_channel<void(error_code)> collection_channel_;

    void process_status_change(connection_status old_status, connection_status new_status)
    {
        // Update the iddle list if required
        if (new_status == connection_status::iddle && old_status != connection_status::iddle)
            shared_st_->iddle_list.add_one(*this);
        else if (new_status != connection_status::iddle && old_status == connection_status::iddle)
            shared_st_->iddle_list.remove(*this);

        // Update the number of pending connections if required
        if (!is_pending(old_status) && is_pending(new_status))
            ++shared_st_->num_pending_connections;
        else if (is_pending(old_status) && !is_pending(new_status))
            --shared_st_->num_pending_connections;
    }

    struct connection_task_op : asio::coroutine
    {
        connection_node& node_;

        connection_task_op(connection_node& node) noexcept : node_(node) {}

        template <class Self>
        void operator()(Self& self, error_code ec = {})
        {
            next_connection_action act;
            collection_state col_st = collection_state::none;
            connection_status new_status, old_status;

            BOOST_ASIO_CORO_REENTER(*this)
            {
                // If we're in thread-safe mode, this is guaranteed to be invoked
                // from within the pool's strand, and self will have the strand as associated executor
                ++node_.shared_st_->num_pending_connections;
                node_.shared_st_->wait_gp.on_task_start();

                while (true)
                {
                    // Check for cancellation
                    if (node_.cancelled_)
                    {
                        node_.shared_st_->wait_gp.on_task_finish();
                        self.complete(error_code());
                        return;
                    }

                    // Invoke the sans-io algorithm
                    old_status = node_.sansio_impl_.status();
                    act = node_.sansio_impl_.resume(ec, col_st);
                    new_status = node_.sansio_impl_.status();

                    // Submit notifications and whatever is required from the status change
                    node_.process_status_change(old_status, new_status);

                    // Apply the next action
                    if (act == next_connection_action::connect)
                    {
                        // Connect
                        BOOST_ASIO_CORO_YIELD
                        run_with_timeout(
                            node_.timer_,
                            node_.params_->connect_timeout,
                            node_.conn_
                                .async_connect(&node_.params_->connect_config, node_.diag_, asio::deferred),
                            std::move(self)
                        );

                        // Store the result so get_connection can return meaningful diagnostics
                        node_.shared_st_->iddle_list.set_last_error(ec, diagnostics(node_.diag_));
                    }
                    else if (act == next_connection_action::sleep_connect_failed)
                    {
                        // Sleep
                        node_.timer_.expires_after(node_.params_->retry_interval);

                        BOOST_ASIO_CORO_YIELD
                        node_.timer_.async_wait(std::move(self));
                    }
                    else if (act == next_connection_action::ping)
                    {
                        BOOST_ASIO_CORO_YIELD
                        run_with_timeout(
                            node_.timer_,
                            node_.params_->ping_timeout,
                            node_.conn_.async_ping(asio::deferred),
                            std::move(self)
                        );
                    }
                    else if (act == next_connection_action::reset)
                    {
                        BOOST_ASIO_CORO_YIELD
                        run_with_timeout(
                            node_.timer_,
                            node_.params_->ping_timeout,
                            node_.conn_.async_reset_connection(asio::deferred),
                            std::move(self)
                        );
                    }
                    else if (act == next_connection_action::iddle_wait)
                    {
                        BOOST_ASIO_CORO_YIELD
                        run_with_timeout(
                            node_.timer_,
                            node_.params_->ping_interval,
                            node_.collection_channel_.async_receive(asio::deferred),
                            std::move(self)
                        );

                        // Iddle wait may yield a collection state. Load it and pass it to the sans-io
                        // algorithm.
                        col_st = node_.collection_state_.exchange(collection_state::none);
                    }
                    else
                    {
                        BOOST_ASSERT(false);
                    }
                }
            }
        }
    };

public:
    connection_node(
        internal_pool_params& params,
        boost::asio::any_io_executor ex,
        boost::asio::any_io_executor conn_ex,
        conn_shared_state& shared_st
    )
        : params_(&params),
          conn_(std::move(conn_ex), params.make_ctor_params()),
          timer_(ex),
          shared_st_(&shared_st),
          collection_channel_(ex, 1)
    {
    }

    void stop_task()
    {
        cancelled_ = true;
        timer_.cancel();
        collection_channel_.close();
    }

    template <class CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_run(CompletionToken&& token)
    {
        return asio::async_compose<CompletionToken, void(error_code)>(connection_task_op(*this), token);
    }

    any_connection& connection() noexcept { return conn_; }
    const any_connection& connection() const noexcept { return conn_; }

    void mark_as_in_use() noexcept
    {
        BOOST_ASSERT(sansio_impl_.status() == connection_status::iddle);
        sansio_impl_.mark_as_in_use();
        process_status_change(connection_status::iddle, connection_status::in_use);
    }

    // Thread-safe. May be safely be called without getting into the strand.
    void mark_as_collectable(bool should_reset) noexcept
    {
        collection_state_.store(
            should_reset ? collection_state::needs_collect_with_reset : collection_state::needs_collect
        );

        // If, for any reason, this notification fails, the connection will
        // be collected when the next ping is due.
        try
        {
            collection_channel_.try_send(error_code());
        }
        catch (...)
        {
        }
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
