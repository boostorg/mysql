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
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/ssl_mode.hpp>

#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/connection_pool/iddle_connection_list.hpp>
#include <boost/mysql/detail/connection_pool/task_joiner.hpp>

#include <boost/asio/async_result.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/post.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <string>

namespace boost {
namespace mysql {
namespace detail {

// TODO: this should be somewhere public
struct owning_pool_params
{
    any_address address;
    std::string username;
    std::string password;
    std::string database;
    std::uint16_t connection_collation{handshake_params::default_collation};
    ssl_mode ssl;
    bool multi_queries{};
    std::size_t initial_size{1};
    std::size_t max_size{150};
    std::chrono::steady_clock::duration connect_timeout;
    std::chrono::steady_clock::duration ping_timeout;
    std::chrono::steady_clock::duration reset_timeout;
    std::chrono::steady_clock::duration retry_interval;
    std::chrono::steady_clock::duration ping_interval;

    handshake_params hparams() const noexcept
    {
        return handshake_params(username, password, database, connection_collation, ssl, multi_queries);
    }
};

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
    none,
    connect,
    sleep_connect_failed,
    iddle_wait,
    reset,
    ping,
    close
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
    next_connection_action resume(error_code ec, collection_state col_st);

    void mark_as_in_use() noexcept
    {
        BOOST_ASSERT(status_ == connection_status::iddle);
        status_ = connection_status::in_use;
    }

    connection_status status() const noexcept { return status_; }
};

inline error_code to_error_code(
    std::array<std::size_t, 2> completion_order,
    error_code io_ec,
    error_code timer_ec
) noexcept
{
    if (completion_order[0] == 0u)  // I/O finished first
        return io_ec;
    else if (completion_order[1] == 0u && !timer_ec)  // Timer fired. Operation timed out
        return asio::error::timed_out;
    else  // Timer was cancelled
        return asio::error::operation_aborted;
}

class connection_node : public hook_type
{
    // Not thread-safe, should only be manipulated within the connection pool strand context
    const owning_pool_params* params_;
    sansio_connection_node sansio_impl_;
    any_connection conn_;
    boost::asio::steady_timer timer_;
    conn_shared_state* shared_st_;
    diagnostics diag_;

    // Thread-safe, may be called from any thread
    std::atomic<collection_state> collection_state_{collection_state::none};
    asio::experimental::concurrent_channel<void(error_code)> collection_channel_;

    boost::asio::any_io_executor get_strand_executor() { return timer_.get_executor(); }

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

        template <class Self, class Op>
        void run_with_timeout(
            Self& self,
            Op&& op,
            std::chrono::steady_clock::duration timeout,
            bool use_strand_executor
        )
        {
            // Set the timeout
            node_.timer_.expires_after(timeout);

            // Create the group
            auto gp = asio::experimental::make_parallel_group(
                std::forward<Op>(op),
                node_.timer_.async_wait(asio::deferred)
            );

            // Initiate the wait
            if (use_strand_executor)
            {
                gp.async_wait(
                    asio::experimental::wait_for_one(),
                    asio::bind_executor(node_.get_strand_executor(), std::move(self))
                );
            }
            else
            {
                gp.async_wait(asio::experimental::wait_for_one(), std::move(self));
            }
        }

        template <class Self>
        void operator()(
            Self& self,
            std::array<std::size_t, 2> completion_order,
            error_code io_ec,
            error_code timer_ec
        )
        {
            // Completion handler for all parallel_group operations involving
            // an I/O operation returning an error_code (connect, ping, reset or iddle wait)
            // and a timer. Timer must always be second.
            // Transform this into a single error_code we can process easier
            (*this)(self, to_error_code(completion_order, io_ec, timer_ec));
        }

        template <class Self>
        void operator()(Self& self, error_code ec = {})
        {
            next_connection_action act;
            collection_state col_st = collection_state::none;
            connection_status new_status, old_status;

            BOOST_ASIO_CORO_REENTER(*this)
            {
                ++node_.shared_st_->num_pending_connections;
                node_.shared_st_->wait_gp.on_task_start();

                while (true)
                {
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
                            self,
                            node_.conn_.async_connect(
                                node_.params_->address,
                                node_.params_->hparams(),
                                node_.diag_,
                                asio::deferred
                            ),
                            node_.params_->connect_timeout,
                            true
                        );

                        // Store the result so get_connection can return meaningful diagnostics
                        node_.shared_st_->iddle_list.set_last_error(ec, diagnostics(node_.diag_));
                    }
                    else if (act == next_connection_action::sleep_connect_failed)
                    {
                        // Just sleep. The timer already already uses the strand executor,
                        // no need to bind any executor here.
                        node_.timer_.expires_after(node_.params_->retry_interval);

                        BOOST_ASIO_CORO_YIELD
                        node_.timer_.async_wait(std::move(self));
                    }
                    else if (act == next_connection_action::ping)
                    {
                        BOOST_ASIO_CORO_YIELD
                        run_with_timeout(
                            self,
                            node_.conn_.async_ping(asio::deferred),
                            node_.params_->ping_timeout,
                            true
                        );
                    }
                    else if (act == next_connection_action::reset)
                    {
                        BOOST_ASIO_CORO_YIELD
                        run_with_timeout(
                            self,
                            node_.conn_.async_reset_connection(asio::deferred),
                            node_.params_->reset_timeout,
                            true
                        );
                    }
                    else if (act == next_connection_action::iddle_wait)
                    {
                        // Both I/O objects use the strand executor. No need to bind any executor here
                        BOOST_ASIO_CORO_YIELD
                        run_with_timeout(
                            self,
                            node_.collection_channel_.async_receive(asio::deferred),
                            node_.params_->ping_interval,
                            false
                        );

                        // Iddle wait may yield a collection state. Load it and pass it to the sans-io
                        // algorithm.
                        col_st = node_.collection_state_.exchange(collection_state::none);
                    }
                    else if (act == next_connection_action::close)
                    {
                        // This doesn't actually involve I/O
                        node_.conn_.force_close(ec);
                    }
                    else if (act == next_connection_action::none)
                    {
                        node_.shared_st_->wait_gp.on_task_finish();
                        self.complete(error_code());
                        return;
                    }
                }
            }
        }
    };

public:
    connection_node(
        const owning_pool_params& params,
        boost::asio::any_io_executor ex,
        boost::asio::any_io_executor strand_ex,
        conn_shared_state& shared_st
    )
        : params_(&params),
          conn_(ex),
          timer_(strand_ex),
          shared_st_(&shared_st),
          collection_channel_(strand_ex, 1)
    {
    }

    void stop_task()
    {
        timer_.cancel();
        collection_channel_.close();
    }

    template <class CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_run(CompletionToken&& token)
    {
        return asio::async_compose<CompletionToken, void(error_code)>(
            connection_task_op(*this),
            std::forward<CompletionToken>(token)
        );
    }

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
