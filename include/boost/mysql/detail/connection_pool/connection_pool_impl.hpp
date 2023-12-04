//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_CONNECTION_POOL_CONNECTION_POOL_IMPL_HPP
#define BOOST_MYSQL_DETAIL_CONNECTION_POOL_CONNECTION_POOL_IMPL_HPP

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/pool_params.hpp>
#include <boost/mysql/pooled_connection.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/connection_pool/connection_node.hpp>
#include <boost/mysql/detail/connection_pool/idle_connection_list.hpp>
#include <boost/mysql/detail/connection_pool/run_with_timeout.hpp>
#include <boost/mysql/detail/connection_pool/task_joiner.hpp>

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/experimental/channel_error.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/optional/optional.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <list>
#include <memory>

namespace boost {
namespace mysql {
namespace detail {

class connection_pool_impl : public std::enable_shared_from_this<connection_pool_impl>
{
    enum class state_t
    {
        initial,
        running,
        cancelled,
    };

    state_t state_{state_t::initial};
    internal_pool_params params_;
    asio::any_io_executor ex_;
    asio::any_io_executor conn_ex_;
    std::list<connection_node> all_conns_;
    conn_shared_state shared_st_;
    asio::experimental::concurrent_channel<void(error_code)> cancel_chan_;

    boost::asio::any_io_executor get_io_executor() const { return conn_ex_ ? conn_ex_ : ex_; }

    void create_connection()
    {
        all_conns_.emplace_back(params_, ex_, conn_ex_, shared_st_);
        all_conns_.back().async_run(asio::bind_executor(ex_, asio::detached));
    }

    struct run_op : asio::coroutine
    {
        std::shared_ptr<connection_pool_impl> obj_;

        run_op(std::shared_ptr<connection_pool_impl> obj) noexcept : obj_(std::move(obj)) {}

        template <class Self>
        void operator()(Self& self, error_code ec = {})
        {
            // TODO: per-operation cancellation here doesn't do the right thing
            boost::ignore_unused(ec);
            BOOST_ASIO_CORO_REENTER(*this)
            {
                // Ensure we run within the pool executor (possibly a strand)
                BOOST_ASIO_CORO_YIELD
                asio::dispatch(obj_->ex_, std::move(self));

                // TODO: we could support running twice
                BOOST_ASSERT(obj_->state_ == state_t::initial);
                obj_->state_ = state_t::running;

                // Create the initial connections
                for (std::size_t i = 0; i < obj_->params_.initial_size; ++i)
                    obj_->create_connection();

                // Wait for the cancel notification to arrive
                BOOST_ASIO_CORO_YIELD
                obj_->cancel_chan_.async_receive(std::move(self));

                // Deliver the cancel notification to all other tasks
                obj_->state_ = state_t::cancelled;
                obj_->shared_st_.idle_list.close_channel();
                for (auto& conn : obj_->all_conns_)
                    conn.stop_task();

                // Wait for all connection tasks to exit
                BOOST_ASIO_CORO_YIELD
                obj_->shared_st_.wait_gp.join_tasks(std::move(self));

                // Done
                obj_.reset();
                self.complete(error_code());
            }
        }
    };

    struct get_connection_op : asio::coroutine
    {
        std::shared_ptr<connection_pool_impl> obj_;
        boost::optional<std::chrono::steady_clock::time_point> timeout_tp_;
        diagnostics* diag_;
        std::shared_ptr<asio::steady_timer> timer_;

        get_connection_op(
            std::shared_ptr<connection_pool_impl> obj,
            std::chrono::steady_clock::duration timeout,
            diagnostics* diag
        ) noexcept
            : obj_(std::move(obj)), diag_(diag)
        {
            if (timeout.count() > 0)
                timeout_tp_ = std::chrono::steady_clock::now() + timeout;
        }

        template <class Self>
        void do_complete(Self& self, error_code ec, pooled_connection conn)
        {
            timer_.reset();
            obj_.reset();
            self.complete(ec, std::move(conn));
        }

        template <class Self>
        void complete_success(Self& self, connection_node& node)
        {
            node.mark_as_in_use();
            do_complete(self, error_code(), access::construct<pooled_connection>(node, std::move(obj_)));
        }

        template <class Self>
        void operator()(Self& self, error_code ec = {})
        {
            connection_node* node{};

            BOOST_ASIO_CORO_REENTER(*this)
            {
                // Clear diagnostics
                if (diag_)
                    diag_->clear();

                // Ensure we run within the pool's executor (or the handler's) (possibly a strand)
                BOOST_ASIO_CORO_YIELD
                asio::post(obj_->ex_, std::move(self));

                // If we're not running yet, or were cancelled, just return
                if (obj_->state_ != state_t::running)
                {
                    do_complete(self, client_errc::cancelled, pooled_connection());
                    return;
                }

                // Try to get a connection without blocking
                node = obj_->shared_st_.idle_list.try_get_one();
                if (node)
                {
                    // There was a connection. Done.
                    complete_success(self, *node);
                    return;
                }

                // No luck. If there is room for more connections, create one.
                // Don't create new connections if we have other connections pending
                // (i.e. being connected, reset... ) - otherwise pool size increases for
                // no reason when there is no connectivity.
                if (obj_->all_conns_.size() < obj_->params_.max_size &&
                    obj_->shared_st_.num_pending_connections == 0u)
                {
                    obj_->create_connection();
                }

                // Allocate a timer to perform waits.
                if (timeout_tp_)
                {
                    timer_ = std::allocate_shared<asio::steady_timer>(
                        asio::get_associated_allocator(self),
                        obj_->ex_
                    );
                }

                // Wait for a connection to become idle and return it
                while (true)
                {
                    // Wait to be notified, or until a timeout happens
                    BOOST_ASIO_CORO_YIELD
                    run_with_timeout(
                        timer_.get(),
                        timeout_tp_,
                        obj_->shared_st_.idle_list.async_wait(asio::deferred),
                        std::move(self)
                    );

                    // Check result
                    if (ec)
                    {
                        if (ec == client_errc::timeout && obj_->shared_st_.idle_list.last_error())
                        {
                            // The operation timed out. Attempt to provide as better diagnostics as we can.
                            // If no diagnostics are available, just leave the timeout error as-is.
                            ec = obj_->shared_st_.idle_list.last_error();
                            if (diag_)
                                *diag_ = obj_->shared_st_.idle_list.last_diagnostics();
                        }
                        else if (ec == asio::experimental::channel_errc::channel_closed)
                        {
                            // Having the channel closed means that the pool is no longer running
                            ec = client_errc::cancelled;
                        }

                        do_complete(self, ec, pooled_connection());
                        return;
                    }

                    // Attempt to get a node. This will almost likely succeed,
                    // but the loop guards against possible race conditions.
                    node = obj_->shared_st_.idle_list.try_get_one();
                    if (node)
                    {
                        complete_success(self, *node);
                        return;
                    }
                }
            }
        }
    };

public:
    connection_pool_impl(const pool_executor_params& ex_params, pool_params&& params)
        : params_(make_internal_pool_params(std::move(params))),
          ex_(ex_params.pool_executor()),
          conn_ex_(ex_params.connection_executor()),
          shared_st_(get_io_executor()),
          cancel_chan_(get_io_executor(), 1)
    {
    }

    using executor_type = asio::any_io_executor;

    executor_type get_executor() { return ex_; }

    template <class CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_run(CompletionToken&& token)
    {
        return asio::async_compose<CompletionToken, void(error_code)>(run_op(shared_from_this()), token, ex_);
    }

    void cancel()
    {
        // This is a concurrent channel, no need to dispatch to the strand
        cancel_chan_.try_send(error_code());
    }

    template <class CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, pooled_connection))
    async_get_connection(
        std::chrono::steady_clock::duration timeout,
        diagnostics* diag,
        CompletionToken&& token
    )
    {
        BOOST_ASSERT(timeout.count() >= 0);
        return asio::async_compose<CompletionToken, void(error_code, pooled_connection)>(
            get_connection_op(shared_from_this(), timeout, diag),
            token,
            ex_
        );
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
