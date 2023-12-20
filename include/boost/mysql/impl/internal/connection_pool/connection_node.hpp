//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_CONNECTION_POOL_CONNECTION_NODE_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_CONNECTION_POOL_CONNECTION_NODE_HPP

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/connection_pool_fwd.hpp>

#include <boost/mysql/impl/internal/connection_pool/internal_pool_params.hpp>
#include <boost/mysql/impl/internal/connection_pool/sansio_connection_node.hpp>
#include <boost/mysql/impl/internal/connection_pool/timer_list.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/intrusive/list.hpp>
#include <boost/intrusive/list_hook.hpp>

namespace boost {
namespace mysql {
namespace detail {

// Traits to use by default for nodes. Templating on traits provides
// a way to mock dependencies in tests. Production code only uses
// instantiations that use io_traits.
// Having this as a traits type (as opposed to individual template params)
// allows us to forward-declare io_traits without having to include steady_timer
struct io_traits
{
    using connection_type = any_connection;
    using timer_type = asio::steady_timer;
};

// State shared between connection tasks
template <class IoTraits>
struct conn_shared_state
{
    intrusive::list<basic_connection_node<IoTraits>> idle_list;
    timer_list<typename IoTraits::timer_type> pending_requests;
    std::size_t num_pending_connections{0};
    error_code last_ec;
    diagnostics last_diag;
};

// Used when launching an op and a timer in parallel using make_parallel_group.
// Translates the completion handler arguments into a single error code.
// Timer must always be the 2nd argument.
inline error_code to_error_code(
    std::array<std::size_t, 2> completion_order,
    error_code io_ec,
    error_code timer_ec
) noexcept
{
    if (completion_order[0] == 0u)  // I/O finished first
        return io_ec;
    else if (completion_order[1] == 0u && !timer_ec)  // Timer fired. Operation timed out
        return client_errc::timeout;
    else  // Timer was cancelled
        return client_errc::cancelled;
}

// The templated type is never exposed to the user. We template
// so tests can inject mocks.
template <class IoTraits>
class basic_connection_node : public intrusive::list_base_hook<>,
                              public sansio_connection_node<basic_connection_node<IoTraits>>
{
    using this_type = basic_connection_node<IoTraits>;
    using connection_type = typename IoTraits::connection_type;
    using timer_type = typename IoTraits::timer_type;

    // Not thread-safe, must be manipulated within the pool's executor
    const internal_pool_params* params_;
    conn_shared_state<IoTraits>* shared_st_;
    connection_type conn_;
    timer_type timer_;
    diagnostics connect_diag_;

    // Thread-safe
    std::atomic<collection_state> collection_state_{collection_state::none};
    asio::experimental::concurrent_channel<void(error_code)> collection_channel_;

    // Hooks for sansio_connection_node
    friend class sansio_connection_node<basic_connection_node<IoTraits>>;
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
        shared_st_->last_ec = ec;
        shared_st_->last_diag = connect_diag_;
    }

    struct connection_task_op
    {
        this_type& node_;
        next_connection_action last_act_{next_connection_action::none};

        connection_task_op(this_type& node) noexcept : node_(node) {}

        // Runs Op with the given timeout, using the connection node timer.
        // If timeout is zero, the op is launched without a timeout.
        // Executor binding is the caller's resposibility.
        template <class Op, class Self>
        void run_with_timeout(Self& self, std::chrono::steady_clock::duration timeout, Op&& op)
        {
            if (timeout.count() > 0)
            {
                node_.timer_.expires_after(timeout);
                asio::experimental::make_parallel_group(
                    std::forward<Op>(op),
                    node_.timer_.async_wait(asio::deferred)
                )
                    .async_wait(asio::experimental::wait_for_one(), std::move(self));
            }
            else
            {
                std::forward<Op>(op)(std::move(self));
            }
        }

        // Handler for parallel group operations. Converts the args into a single error
        // code and call the main handler.
        template <class Self>
        void operator()(
            Self& self,
            std::array<std::size_t, 2> completion_order,
            error_code io_ec,
            error_code timer_ec
        )
        {
            self(to_error_code(completion_order, io_ec, timer_ec));
        }

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

            // Apply the next action.
            // Recall that the connection's executor may be different from the pool's.
            // When passing a token with an executor to parallel_group::async_wait,
            // only the final handler uses that executor, and not individual operation handlers.
            // Thus, all connection operations must be bound to the pool's executor before being
            // passed to run_with_timeout.
            switch (last_act_)
            {
            case next_connection_action::connect:
                run_with_timeout(
                    self,
                    node_.params_->connect_timeout,
                    asio::bind_executor(
                        node_.timer_.get_executor(),
                        node_.conn_.async_connect(
                            &node_.params_->connect_config,
                            node_.connect_diag_,
                            asio::deferred
                        )
                    )
                );
                break;
            case next_connection_action::sleep_connect_failed:
                node_.timer_.expires_after(node_.params_->retry_interval);
                node_.timer_.async_wait(std::move(self));
                break;
            case next_connection_action::ping:
                run_with_timeout(
                    self,
                    node_.params_->ping_timeout,
                    asio::bind_executor(node_.timer_.get_executor(), node_.conn_.async_ping(asio::deferred))
                );
                break;
            case next_connection_action::reset:
                run_with_timeout(
                    self,
                    node_.params_->ping_timeout,
                    asio::bind_executor(
                        node_.timer_.get_executor(),
                        node_.conn_.async_reset_connection(asio::deferred)
                    )
                );
                break;
            case next_connection_action::idle_wait:
                run_with_timeout(
                    self,
                    node_.params_->ping_interval,
                    node_.collection_channel_.async_receive(asio::deferred)
                );
                break;
            case next_connection_action::none: self.complete(error_code()); break;
            default: BOOST_ASSERT(false);
            }
        }
    };

public:
    basic_connection_node(
        internal_pool_params& params,
        boost::asio::any_io_executor ex,
        boost::asio::any_io_executor conn_ex,
        conn_shared_state<IoTraits>& shared_st
    )
        : params_(&params),
          shared_st_(&shared_st),
          conn_(std::move(conn_ex), params.make_ctor_params()),
          timer_(ex),
          collection_channel_(ex, 1)
    {
    }

    void cancel()
    {
        sansio_connection_node<this_type>::cancel();
        timer_.cancel();
        collection_channel_.close();
    }

    // This initiation must be invoked within the pool's executor
    template <class CompletionToken>
    auto async_run(CompletionToken&& token)
        -> decltype(asio::async_compose<CompletionToken, void(error_code)>(connection_task_op{*this}, token))
    {
        return asio::async_compose<CompletionToken, void(error_code)>(connection_task_op{*this}, token);
    }

    connection_type& connection() noexcept { return conn_; }
    const connection_type& connection() const noexcept { return conn_; }

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

    // Exposed for testing
    collection_state get_collection_state() const noexcept { return collection_state_; }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
