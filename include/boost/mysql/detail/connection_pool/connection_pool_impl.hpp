//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_CONNECTION_POOL_CONNECTION_POOL_IMPL_HPP
#define BOOST_MYSQL_DETAIL_CONNECTION_POOL_CONNECTION_POOL_IMPL_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/pooled_connection.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/connection_pool/connection_node.hpp>
#include <boost/mysql/detail/connection_pool/iddle_connection_list.hpp>
#include <boost/mysql/detail/connection_pool/task_joiner.hpp>

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/core/ignore_unused.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <list>
#include <memory>

namespace boost {
namespace mysql {
namespace detail {

class connection_pool_impl
{
    bool cancelled_{};
    pool_params params_;
    asio::any_io_executor ex_;
    asio::any_io_executor strand_ex_;
    std::list<connection_node> all_conns_;
    conn_shared_state shared_st_;
    asio::experimental::concurrent_channel<void(error_code)> cancel_chan_;

    boost::asio::any_io_executor get_io_executor() const { return strand_ex_ ? strand_ex_ : ex_; }

    void create_connection()
    {
        all_conns_.emplace_back(params_, ex_, get_io_executor(), shared_st_);
        all_conns_.back().async_run(asio::detached);
    }

    bool is_thread_safe() const noexcept { return params_.enable_thread_safety; }

    struct run_op : asio::coroutine
    {
        connection_pool_impl& obj_;

        run_op(connection_pool_impl& obj) noexcept : obj_(obj) {}

        template <class Self>
        void operator()(Self& self, error_code ec = {})
        {
            BOOST_ASSERT(!ec);
            ignore_unused(ec);

            BOOST_ASIO_CORO_REENTER(*this)
            {
                // Ensure we run within the strand (if thread-safety enabled)
                if (obj_.is_thread_safe())
                {
                    BOOST_ASIO_CORO_YIELD
                    asio::dispatch(obj_.strand_ex_, std::move(self));
                }

                // TODO: check that we're not running
                // TODO: should we support running twice?

                // Create the initial connections
                for (std::size_t i = 0; i < obj_.params_.initial_size; ++i)
                    obj_.create_connection();

                // Wait for the cancel notification to arrive
                BOOST_ASIO_CORO_YIELD
                obj_.cancel_chan_.async_receive(std::move(self));

                // Deliver the cancel notification to all other tasks
                obj_.cancelled_ = true;
                obj_.shared_st_.iddle_list.close_channel();
                for (auto& conn : obj_.all_conns_)
                    conn.stop_task();

                // Wait for all connection tasks to exit
                BOOST_ASIO_CORO_YIELD
                obj_.shared_st_.wait_gp.join_tasks(std::move(self));

                // Done
                self.complete(error_code());
            }
        }
    };

    struct get_connection_op : asio::coroutine
    {
        connection_pool_impl& obj_;
        std::chrono::steady_clock::time_point timeout_tp_;
        diagnostics* diag_;
        connection_node* result_;
        std::unique_ptr<asio::steady_timer> timer_;

        get_connection_op(
            connection_pool_impl& obj,
            std::chrono::steady_clock::duration timeout,
            diagnostics* diag
        ) noexcept
            : obj_(obj), timeout_tp_(std::chrono::steady_clock::now() + timeout), diag_(diag)
        {
        }

        template <class Self>
        void do_complete(Self& self, error_code ec, pooled_connection conn)
        {
            timer_.reset();
            self.complete(ec, std::move(conn));
        }

        template <class Self>
        void complete_success(Self& self)
        {
            BOOST_ASSERT(result_ != nullptr);
            result_->mark_as_in_use();
            do_complete(self, error_code(), access::construct<pooled_connection>(*result_));
        }

        template <class Self>
        void operator()(
            Self& self,
            std::array<std::size_t, 2> completion_order,
            error_code channel_ec,
            error_code timer_ec
        )
        {
            (*this)(self, to_error_code(completion_order, channel_ec, timer_ec));
        }

        template <class Self>
        void operator()(Self& self, error_code ec = {})
        {
            BOOST_ASIO_CORO_REENTER(*this)
            {
                // Clear diagnostics
                if (diag_)
                    diag_->clear();

                // Ensure we run within the strand (if thread-safety enabled)
                if (obj_.is_thread_safe())
                {
                    BOOST_ASIO_CORO_YIELD
                    asio::dispatch(obj_.strand_ex_, std::move(self));
                }

                // Double check that we weren't cancelled
                if (obj_.cancelled_)
                {
                    BOOST_ASIO_CORO_YIELD
                    asio::post(obj_.ex_, std::move(self));

                    do_complete(self, asio::error::operation_aborted, pooled_connection());
                    return;
                }

                // Try to get a connection without blocking
                // TODO: could we expose a try_get_connection? is this useful?
                result_ = obj_.shared_st_.iddle_list.try_get_one();
                if (result_)
                {
                    // There was a connection. Done.
                    BOOST_ASIO_CORO_YIELD
                    asio::post(obj_.ex_, std::move(self));

                    complete_success(self);
                    return;
                }

                // No luck. If there is room for more connections, create one.
                // Don't create new connections if we have other connections pending
                // (i.e. being connected, reset... ) - otherwise pool size increases for
                // no reason when there is no connectivity.
                if (obj_.all_conns_.size() < obj_.params_.max_size &&
                    obj_.shared_st_.num_pending_connections == 0u)
                {
                    obj_.create_connection();
                }

                // Allocate a timer to perform waits.
                // TODO: do this correctly
                timer_.reset(new asio::steady_timer(obj_.get_io_executor()));

                // Wait for a connection to become iddle and return it
                while (true)
                {
                    // Wait to be notified, or until a timeout happens
                    timer_->expires_at(timeout_tp_);

                    BOOST_ASIO_CORO_YIELD
                    asio::experimental::make_parallel_group(
                        obj_.shared_st_.iddle_list.async_wait(asio::deferred),
                        timer_->async_wait(asio::deferred)
                    )
                        .async_wait(asio::experimental::wait_for_one(), std::move(self));

                    // Check result
                    if (ec)
                    {
                        if (ec == asio::error::timed_out && obj_.shared_st_.iddle_list.last_error())
                        {
                            // The operation timed out. Attempt to provide as better diagnostics as we can.
                            // If no diagnostics are available, just leave the timeout error as-is.
                            ec = obj_.shared_st_.iddle_list.last_error();
                            if (diag_)
                                *diag_ = obj_.shared_st_.iddle_list.last_diagnostics();
                        }

                        if (obj_.is_thread_safe())
                        {
                            BOOST_ASIO_CORO_YIELD
                            asio::post(obj_.ex_, std::move(self));
                        }

                        do_complete(self, ec, pooled_connection());
                        return;
                    }

                    // Attempt to get a node. This will almost likely succeed,
                    // but the loop guards against possible race conditions.
                    result_ = obj_.shared_st_.iddle_list.try_get_one();
                    if (result_)
                    {
                        // If we're in thread-safe mode, we're running within the
                        // strand. Make sure we exit, so the final handler is not called
                        // within the strand.
                        if (obj_.is_thread_safe())
                        {
                            BOOST_ASIO_CORO_YIELD
                            asio::post(obj_.ex_, std::move(self));
                        }

                        complete_success(self);
                        return;
                    }
                }
            }
        }
    };

public:
    connection_pool_impl(pool_params&& params, asio::any_io_executor ex)
        : params_(std::move(params)),
          ex_(std::move(ex)),
          strand_ex_(params_.enable_thread_safety ? asio::make_strand(ex_) : asio::any_io_executor()),
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
        return asio::async_compose<CompletionToken, void(error_code)>(run_op(*this), token, ex_);
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
        return asio::async_compose<CompletionToken, void(error_code, pooled_connection)>(
            get_connection_op(*this, timeout, diag),
            token,
            ex_
        );
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
