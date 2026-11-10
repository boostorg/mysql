//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_CONNECTION_POOL_CONNECTION_NODE_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_CONNECTION_POOL_CONNECTION_NODE_HPP

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/character_set.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/pipeline.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/connection_pool_fwd.hpp>

#include <boost/mysql/impl/internal/connection_pool/internal_pool_params.hpp>
#include <boost/mysql/impl/internal/connection_pool/sansio_connection_node.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/asio/cancel_after.hpp>
#include <boost/asio/cancel_at.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/error.hpp>
#include <boost/intrusive/list.hpp>
#include <boost/intrusive/list_hook.hpp>

#include <algorithm>
#include <chrono>
#include <utility>
#include <vector>

namespace boost {
namespace mysql {
namespace detail {

// State shared between connection tasks
template <class ConnectionType, class ClockType>
struct conn_shared_state
{
    // The list of connections that are currently idle. Non-owning.
    intrusive::list<basic_connection_node<ConnectionType, ClockType>> idle_list;

    // Timer acting as a condition variable to wait for idle connections
    asio::basic_waitable_timer<ClockType> idle_connections_cv;

    // The number of pending connections (currently getting ready).
    // Required to compute how many connections we should create at any given point in time.
    std::size_t num_pending_connections{0};

    // The number of async_get_connection ops that are waiting for a connection to become available.
    // Required to compute how many connections we should create at any given point in time.
    std::size_t num_pending_requests{0};

    // Info about the last connection attempt. Already processed, suitable to be used
    // as the result of an async_get_connection op
    diagnostics last_connect_diag;

    // The number of running connections, to track when they exit
    std::size_t num_running_connections{0};

    // The number of connections closing because they were idle for too long, until they're
    // removed from the pool. They will never be handed out again, so they don't count
    // towards the pool size, neither when creating connections nor when closing more.
    std::size_t num_closing_connections{0};

    // Timer acting as a condition variable to wait for all connections to exit.
    // Expired while no connection is running
    asio::basic_waitable_timer<ClockType> conns_finished_cv;

    conn_shared_state(asio::any_io_executor ex)
        : idle_connections_cv(ex, (ClockType::time_point::max)()),
          conns_finished_cv(std::move(ex), (ClockType::time_point::min)())
    {
    }

    void on_connection_start()
    {
        // Re-arm the latch, so async_run waits for this connection, too
        if (num_running_connections++ == 0u)
            conns_finished_cv.expires_at((ClockType::time_point::max)());
    }

    void on_connection_finish()
    {
        if (--num_running_connections == 0u)
            conns_finished_cv.expires_at((ClockType::time_point::min)());
    }
};

// The templated type is never exposed to the user. We template
// so tests can inject mocks.
template <class ConnectionType, class ClockType>
class basic_connection_node : public intrusive::list_base_hook<>,
                              public sansio_connection_node<basic_connection_node<ConnectionType, ClockType>>
{
    using this_type = basic_connection_node<ConnectionType, ClockType>;
    using timer_type = asio::basic_waitable_timer<ClockType>;
    using time_point = typename ClockType::time_point;

    // Not thread-safe
    const internal_pool_params* params_;
    conn_shared_state<ConnectionType, ClockType>* shared_st_;
    ConnectionType conn_;
    timer_type timer_;
    diagnostics connect_diag_;
    timer_type collection_timer_;  // Notifications about collections. A separate timer makes potential race
                                   // conditions not harmful
    std::vector<stage_response> reset_pipeline_res_;
    time_point close_deadline_{};  // When to close the connection if it's still idle and no longer needed
    time_point ping_deadline_{};   // When the next ping is due

    // Thread-safe
    std::atomic<collection_state> collection_state_{collection_state::none};

    // Hooks for sansio_connection_node
    friend class sansio_connection_node<this_type>;
    void entering_idle()
    {
        auto now = ClockType::now();

        // status() still holds the status we're leaving. Pings don't restart the idle period,
        // otherwise an idle_timeout longer than ping_interval could never elapse
        if (this->status() != node_status::ping_in_progress)
            close_deadline_ = deadline_after(now, params_->idle_timeout);
        ping_deadline_ = deadline_after(now, params_->ping_interval);

        shared_st_->idle_list.push_back(*this);
        shared_st_->idle_connections_cv.cancel_one();
    }
    void exiting_idle() { shared_st_->idle_list.erase(shared_st_->idle_list.iterator_to(*this)); }
    void entering_pending() { ++shared_st_->num_pending_connections; }
    void exiting_pending() { --shared_st_->num_pending_connections; }
    void entering_closing() { ++shared_st_->num_closing_connections; }

    // now + interval, or never if the interval is zero (disabled). pool_params accepts
    // duration::max, so the addition saturates instead of overflowing
    static time_point deadline_after(time_point now, std::chrono::steady_clock::duration interval)
    {
        bool never = interval.count() == 0 ||
                     now.time_since_epoch() >= (time_point::duration::max)() - interval;
        return never ? (time_point::max)() : now + interval;
    }

    // Connections already closing are discounted, so simultaneous checks
    // can't shrink the pool below initial_size
    bool should_close_idle() const
    {
        return shared_st_->num_running_connections - shared_st_->num_closing_connections >
                   params_->initial_size &&
               ClockType::now() >= close_deadline_;
    }

    bool should_ping() const { return ClockType::now() >= ping_deadline_; }

    // When the idle wait should end. An idle connection wakes up when a ping is due, or when
    // idle_timeout elapses if that comes first. Once the close deadline has passed, and the
    // connection was kept because of initial_size, only pings wake it up: waiting for a past
    // deadline would spin. A connection in use only waits for the collection notification, but
    // wakes up every hour just in case it was lost (see return_connection)
    time_point wait_deadline() const
    {
        auto now = ClockType::now();
        if (this->status() != node_status::idle)
            return now + std::chrono::hours(1);
        return close_deadline_ > now ? (std::min)(ping_deadline_, close_deadline_) : ping_deadline_;
    }

    // Helpers
    void propagate_connect_diag(error_code ec)
    {
        shared_st_->last_connect_diag = create_connect_diagnostics(ec, connect_diag_);
    }

    template <class Op, class Self>
    void run_with_timeout(Op&& op, std::chrono::steady_clock::duration timeout, Self& self)
    {
        if (timeout.count() > 0)
        {
            std::forward<Op>(op)(asio::cancel_after(timer_, timeout, std::move(self)));
        }
        else
        {
            std::forward<Op>(op)(std::move(self));
        }
    }

    struct connection_task_op
    {
        this_type& node_;
        next_connection_action last_act_{next_connection_action::none};

        connection_task_op(this_type& node) noexcept : node_(node) {}

        template <class Self>
        void operator()(Self& self)
        {
            // Called when the op starts
            node_.shared_st_->on_connection_start();
            (*this)(self, error_code());
        }

        template <class Self>
        void operator()(Self& self, error_code ec)
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
                node_.run_with_timeout(
                    node_.conn_
                        .async_connect(node_.params_->connect_config, node_.connect_diag_, asio::deferred),
                    node_.params_->connect_timeout,
                    self
                );
                break;
            case next_connection_action::sleep_connect_failed:
                node_.timer_.expires_after(node_.params_->retry_interval);
                node_.timer_.async_wait(std::move(self));
                break;
            case next_connection_action::ping:
                node_.run_with_timeout(
                    node_.conn_.async_ping(asio::deferred),
                    node_.params_->ping_timeout,
                    self
                );
                break;
            case next_connection_action::reset:
                node_.run_with_timeout(
                    node_.conn_.async_run_pipeline(
                        node_.params_->reset_pipeline,
                        node_.reset_pipeline_res_,
                        asio::deferred
                    ),
                    node_.params_->ping_timeout,
                    self
                );
                break;
            case next_connection_action::idle_wait:
                // Wait for a collection notification, or until wait_deadline() wakes us up
                node_.collection_timer_.async_wait(
                    asio::cancel_at(node_.timer_, node_.wait_deadline(), std::move(self))
                );
                break;
            case next_connection_action::close:
                node_.run_with_timeout(
                    node_.conn_.async_close(asio::deferred),
                    node_.params_->close_timeout,
                    self
                );
                break;
            case next_connection_action::none:
                node_.shared_st_->on_connection_finish();
                self.complete(error_code());
                break;
            default: BOOST_ASSERT(false);  // LCOV_EXCL_LINE
            }
        }
    };

public:
    basic_connection_node(
        internal_pool_params& params,
        boost::asio::any_io_executor pool_ex,
        boost::asio::any_io_executor conn_ex,
        conn_shared_state<ConnectionType, ClockType>& shared_st
    )
        : params_(&params),
          shared_st_(&shared_st),
          conn_(std::move(conn_ex), params.make_ctor_params()),
          timer_(pool_ex),
          collection_timer_(pool_ex, (ClockType::time_point::max)())
    {
    }

    // Not thread-safe
    void cancel()
    {
        sansio_connection_node<this_type>::cancel();
        timer_.cancel();
        collection_timer_.cancel();
    }

    // Not thread-safe
    template <class CompletionToken>
    auto async_run(CompletionToken&& token)
        -> decltype(asio::async_compose<CompletionToken, void(error_code)>(connection_task_op{*this}, token))
    {
        return asio::async_compose<CompletionToken, void(error_code)>(connection_task_op{*this}, token);
    }

    // Not thread-safe
    void notify_collectable() { collection_timer_.cancel(); }

    // Thread-safe
    void mark_as_collectable(bool should_reset) noexcept
    {
        collection_state_.store(
            should_reset ? collection_state::needs_collect_with_reset : collection_state::needs_collect
        );
    }

    // Getter, used by pooled_connection
    ConnectionType& connection() noexcept { return conn_; }

    // Exposed for testing
    collection_state get_collection_state() const noexcept { return collection_state_; }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
