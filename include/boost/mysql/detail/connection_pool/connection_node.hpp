//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_CONNECTION_POOL_CONNECTION_NODE_HPP
#define BOOST_MYSQL_DETAIL_CONNECTION_POOL_CONNECTION_NODE_HPP

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/connection_pool/internal_pool_params.hpp>
#include <boost/mysql/detail/connection_pool/intrusive_list.hpp>
#include <boost/mysql/detail/connection_pool/run_with_timeout.hpp>
#include <boost/mysql/detail/connection_pool/sansio_connection_node.hpp>
#include <boost/mysql/detail/connection_pool/wait_group.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/steady_timer.hpp>

#include <chrono>

namespace boost {
namespace mysql {
namespace detail {

// Forward decl. for convenience, used by pooled_connection
class connection_pool_impl;

class connection_node;

// State shared between connection tasks
struct conn_shared_state
{
    intrusive_list<connection_node> idle_list;
    asio::experimental::channel<void(error_code)> idle_notification_chan;
    std::size_t num_pending_connections{0};
    error_code last_ec;
    diagnostics last_diag;

    conn_shared_state(boost::asio::any_io_executor ex) : idle_notification_chan(std::move(ex), 1) {}
};

// I/O operations called by connection_node
class connection_node_io
{
    any_connection conn_;
    asio::steady_timer timer_;
    asio::experimental::concurrent_channel<void(error_code)> collection_channel_;

public:
    connection_node_io(
        asio::any_io_executor pool_ex,
        asio::any_io_executor conn_ex,
        any_connection_params ctor_params
    )
        : conn_(std::move(conn_ex), ctor_params), timer_(pool_ex), collection_channel_(std::move(pool_ex), 1)
    {
    }

    template <class CompletionToken>
    void async_connect(
        std::chrono::steady_clock::duration timeout,
        const connect_params& connect_config,
        diagnostics& diag,
        CompletionToken&& token
    )
    {
        run_with_timeout(
            timer_,
            timeout,
            conn_.async_connect(&connect_config, diag, asio::deferred),
            std::forward<CompletionToken>(token)
        );
    }

    template <class CompletionToken>
    void async_sleep(std::chrono::steady_clock::duration timeout, CompletionToken&& token)
    {
        timer_.expires_after(timeout);
        timer_.async_wait(std::forward<CompletionToken>(token));
    }

    template <class CompletionToken>
    void async_ping(std::chrono::steady_clock::duration timeout, CompletionToken&& token)
    {
        run_with_timeout(
            timer_,
            timeout,
            conn_.async_ping(asio::deferred),
            std::forward<CompletionToken>(token)
        );
    }

    template <class CompletionToken>
    void async_reset(std::chrono::steady_clock::duration timeout, CompletionToken&& token)
    {
        run_with_timeout(
            timer_,
            timeout,
            conn_.async_reset_connection(asio::deferred),
            std::forward<CompletionToken>(token)
        );
    }

    template <class CompletionToken>
    void async_idle_wait(std::chrono::steady_clock::duration timeout, CompletionToken&& token)
    {
        run_with_timeout(
            timer_,
            timeout,
            collection_channel_.async_receive(asio::deferred),
            std::forward<CompletionToken>(token)
        );
    }

    any_connection& connection() noexcept { return conn_; }
    const any_connection& connection() const noexcept { return conn_; }

    void notify_collectable() { collection_channel_.try_send(error_code()); }

    void cancel()
    {
        timer_.cancel();
        collection_channel_.close();
    }
};

class connection_node : public list_node, public sansio_connection_node<connection_node>
{
    const internal_pool_params* params_;
    conn_shared_state* shared_st_;
    connection_node_io io_;
    diagnostics connect_diag_;
    std::atomic<collection_state> collection_state_{collection_state::none};

    // Hooks for sansio_connection_node
    friend class sansio_connection_node<connection_node>;
    void entering_idle()
    {
        shared_st_->idle_list.push_back(*this);
        shared_st_->idle_notification_chan.try_send(error_code());
    }
    void exiting_idle() { shared_st_->idle_list.erase(*this); }
    void entering_pending() { ++shared_st_->num_pending_connections; }
    void exiting_pending() { --shared_st_->num_pending_connections; }

    // Helpers
    void propagate_connect_diag(error_code ec)
    {
        shared_st_->last_ec = ec;
        shared_st_->last_diag = connect_diag_;
    }

    struct connection_task_op
    {
        connection_node& node_;
        next_connection_action last_act_{next_connection_action::none};

        connection_task_op(connection_node& node) noexcept : node_(node) {}

        template <class Self>
        void operator()(Self& self, error_code ec = {})
        {
            // A collection status may be generated by idle_wait actions
            auto col_st = last_act_ == next_connection_action::idle_wait
                              ? node_.collection_state_.exchange(collection_state::none)
                              : collection_state::none;

            // Connect actions should set the shared diagnostics, so these
            // get reported to the user
            if (last_act_ == next_connection_action::connect)
                node_.propagate_connect_diag(ec);

            // Invoke the sans-io algorithm
            last_act_ = node_.resume(ec, col_st);

            // Apply the next action
            switch (last_act_)
            {
            case next_connection_action::connect:
                node_.io_.async_connect(
                    node_.params_->connect_timeout,
                    node_.params_->connect_config,
                    node_.connect_diag_,
                    std::move(self)
                );
                break;
            case next_connection_action::sleep_connect_failed:
                node_.io_.async_sleep(node_.params_->retry_interval, std::move(self));
                break;
            case next_connection_action::ping:
                node_.io_.async_ping(node_.params_->ping_timeout, std::move(self));
                break;
            case next_connection_action::reset:
                node_.io_.async_reset(node_.params_->ping_timeout, std::move(self));
                break;
            case next_connection_action::idle_wait:
                node_.io_.async_idle_wait(node_.params_->ping_interval, std::move(self));
                break;
            case next_connection_action::none: self.complete(error_code()); break;
            default: BOOST_ASSERT(false);
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
          shared_st_(&shared_st),
          io_(std::move(ex), std::move(conn_ex), params.make_ctor_params())
    {
    }

    void cancel()
    {
        sansio_connection_node<connection_node>::cancel();
        io_.cancel();
    }

    // This initiation must be invoked within the pool's executor
    template <class CompletionToken>
    void async_run(CompletionToken&& token)
    {
        return asio::async_compose<CompletionToken, void(error_code)>(connection_task_op{*this}, token);
    }

    // Invokes async_run calling the adquate group functions on start/exit
    // and using the group's executor
    void async_run_with_group(wait_group& gp)
    {
        gp.on_task_start();
        async_run(asio::bind_executor(gp.get_executor(), [&gp](error_code) { gp.on_task_finish(); }));
    }

    any_connection& connection() noexcept { return io_.connection(); }
    const any_connection& connection() const noexcept { return io_.connection(); }

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
            io_.notify_collectable();
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
