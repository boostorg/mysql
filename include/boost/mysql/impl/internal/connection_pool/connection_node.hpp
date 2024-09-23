//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
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
#include <boost/mysql/impl/internal/connection_pool/run_with_timeout.hpp>
#include <boost/mysql/impl/internal/connection_pool/sansio_connection_node.hpp>
#include <boost/mysql/impl/internal/connection_pool/timer_list.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/intrusive/list.hpp>
#include <boost/intrusive/list_hook.hpp>

#include <chrono>
#include <utility>
#include <vector>

namespace boost {
namespace mysql {
namespace detail {

// Composes a diagnostics object containing info about the last connect error.
// Suitable for the diagnostics output of async_get_connection
inline diagnostics create_last_connect_diag(error_code connect_ec, const diagnostics& connect_diag)
{
    diagnostics res;
    if (connect_ec)
    {
        // Manipulating the internal representations is more efficient here,
        // and better than using stringstream
        auto& res_impl = access::get_impl(res);
        const auto& connect_diag_impl = access::get_impl(connect_diag);

        if (connect_ec == asio::error::operation_aborted)
        {
            // operation_aborted in this context means timeout
            res_impl.msg = "Last connection attempt timed out";
            res_impl.is_server = false;
        }
        else
        {
            // Add the error code information
            res_impl.msg = "Last connection attempt failed with error code ";
            res_impl.msg += connect_ec.to_string();

            // Add any diagnostics
            if (connect_diag_impl.msg.empty())
            {
                // The resulting object doesn't contain server-supplied info
                res_impl.is_server = false;
            }
            else
            {
                // The resulting object may contain server-supplied info
                res_impl.msg += ": ";
                res_impl.msg += connect_diag_impl.msg;
                res_impl.is_server = connect_diag_impl.is_server;
            }
        }
    }
    return res;
}

// State shared between connection tasks
template <class ConnectionType, class ClockType>
struct conn_shared_state
{
    intrusive::list<basic_connection_node<ConnectionType, ClockType>> idle_list;
    timer_list<ClockType> pending_requests;
    std::size_t num_pending_connections{0};
    diagnostics last_connect_diag;

    conn_shared_state(asio::any_io_executor ex) : pending_requests(std::move(ex)) {}
};

// The templated type is never exposed to the user. We template
// so tests can inject mocks.
template <class ConnectionType, class ClockType>
class basic_connection_node : public intrusive::list_base_hook<>,
                              public sansio_connection_node<basic_connection_node<ConnectionType, ClockType>>
{
    using this_type = basic_connection_node<ConnectionType, ClockType>;
    using timer_type = asio::basic_waitable_timer<ClockType>;

    // Not thread-safe, must be manipulated within the pool's executor
    const internal_pool_params* params_;
    conn_shared_state<ConnectionType, ClockType>* shared_st_;
    ConnectionType conn_;
    timer_type timer_;
    diagnostics connect_diag_;
    timer_type collection_timer_;  // Notifications about collections. A separate timer makes potential race
                                   // conditions not harmful
    const pipeline_request* reset_pipeline_req_;
    std::vector<stage_response> reset_pipeline_res_;

    // Thread-safe
    std::atomic<collection_state> collection_state_{collection_state::none};

    // Hooks for sansio_connection_node
    friend class sansio_connection_node<this_type>;
    void entering_idle()
    {
        shared_st_->idle_list.push_back(*this);
        shared_st_->pending_requests.notify_one();
    }
    void exiting_idle() { shared_st_->idle_list.erase(shared_st_->idle_list.iterator_to(*this)); }
    void entering_pending() { ++shared_st_->num_pending_connections; }
    void exiting_pending() { --shared_st_->num_pending_connections; }

    // Helpers
    void propagate_connect_diag(error_code ec)
    {
        shared_st_->last_connect_diag = create_last_connect_diag(ec, connect_diag_);
    }

    struct connection_task_op
    {
        this_type& node_;
        next_connection_action last_act_{next_connection_action::none};

        connection_task_op(this_type& node) noexcept : node_(node) {}

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

            // Apply the next action. run_with_timeout makes sure that all handlers
            // are dispatched using the timer's executor (that is, the pool executor)
            switch (last_act_)
            {
            case next_connection_action::connect:
                run_with_timeout(
                    node_.conn_
                        .async_connect(node_.params_->connect_config, node_.connect_diag_, asio::deferred),
                    node_.timer_,
                    node_.params_->connect_timeout,
                    std::move(self)
                );
                break;
            case next_connection_action::sleep_connect_failed:
                node_.timer_.expires_after(node_.params_->retry_interval);
                node_.timer_.async_wait(std::move(self));
                break;
            case next_connection_action::ping:
                run_with_timeout(
                    node_.conn_.async_ping(asio::deferred),
                    node_.timer_,
                    node_.params_->ping_timeout,
                    std::move(self)
                );
                break;
            case next_connection_action::reset:
                run_with_timeout(
                    node_.conn_.async_run_pipeline(
                        *node_.reset_pipeline_req_,
                        node_.reset_pipeline_res_,
                        asio::deferred
                    ),
                    node_.timer_,
                    node_.params_->ping_timeout,
                    std::move(self)
                );
                break;
            case next_connection_action::idle_wait:
                run_with_timeout(
                    node_.collection_timer_.async_wait(asio::deferred),
                    node_.timer_,
                    node_.params_->ping_interval,
                    std::move(self)
                );
                break;
            case next_connection_action::none: self.complete(error_code()); break;
            default: BOOST_ASSERT(false);  // LCOV_EXCL_LINE
            }
        }
    };

public:
    basic_connection_node(
        internal_pool_params& params,
        boost::asio::any_io_executor pool_ex,
        boost::asio::any_io_executor conn_ex,
        conn_shared_state<ConnectionType, ClockType>& shared_st,
        const pipeline_request* reset_pipeline_req
    )
        : params_(&params),
          shared_st_(&shared_st),
          conn_(std::move(conn_ex), params.make_ctor_params()),
          timer_(pool_ex),
          collection_timer_(pool_ex, (std::chrono::steady_clock::time_point::max)()),
          reset_pipeline_req_(reset_pipeline_req)
    {
    }

    void cancel()
    {
        sansio_connection_node<this_type>::cancel();
        timer_.cancel();
        collection_timer_.cancel();
    }

    // This initiation must be invoked within the pool's executor
    template <class CompletionToken>
    auto async_run(CompletionToken&& token
    ) -> decltype(asio::async_compose<CompletionToken, void(error_code)>(connection_task_op{*this}, token))
    {
        return asio::async_compose<CompletionToken, void(error_code)>(connection_task_op{*this}, token);
    }

    ConnectionType& connection() noexcept { return conn_; }
    const ConnectionType& connection() const noexcept { return conn_; }

    // Not thread-safe, must be called within the pool's executor
    void notify_collectable() { collection_timer_.cancel(); }

    // Thread-safe. May be safely be called from any thread.
    void mark_as_collectable(bool should_reset) noexcept
    {
        collection_state_.store(
            should_reset ? collection_state::needs_collect_with_reset : collection_state::needs_collect
        );
    }

    // Exposed for testing
    collection_state get_collection_state() const noexcept { return collection_state_; }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
