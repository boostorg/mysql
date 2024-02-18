//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_UNIT_TEST_CONNECTION_POOL_MOCK_TIMER_HPP
#define BOOST_MYSQL_TEST_UNIT_TEST_CONNECTION_POOL_MOCK_TIMER_HPP

#include <boost/mysql/error_code.hpp>

#include <boost/asio/any_completion_executor.hpp>
#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/append.hpp>
#include <boost/asio/associated_cancellation_slot.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/execution_context.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/post.hpp>

#include <chrono>
#include <cstddef>
#include <list>
#include <utility>

namespace boost {
namespace mysql {
namespace test {

// Workaround to keep services header-only
template <int I>
class service_base : public asio::execution_context::service
{
public:
    static const asio::execution_context::id id;
    using asio::execution_context::service::service;
};

template <int I>
const asio::execution_context::id service_base<I>::id;

/**
 * Used by mock timers. Like asio::detail::deadline_timer_service, but for mock timers.
 * Mock timers don't rely on the actual clock, but on a time_point hold by this class.
 * Call mock_timer_service::advance_time_xx to adjust the current time. This will
 * call timer handlers as if time had advanced. Note that we don't have a way to mock
 * steady_clock::now(). Our code under test must make sure not to call it.
 */
class mock_timer_service : public service_base<0>
{
public:
    // Required by all Boost.Asio services
    void shutdown() final {}
    mock_timer_service(asio::execution_context& owner) : service_base<0>(owner) {}

    // A pending timer
    struct pending_timer
    {
        // When does the timer expire?
        std::chrono::steady_clock::time_point expiry;

        // The executor to use
        asio::any_io_executor ex;

        // Maintain work for the executor
        asio::executor_work_guard<asio::any_io_executor> work_guard;

        // What handler should we call?
        asio::any_completion_handler<void(error_code)> handler;

        // Uniquely identifies the timer, so we can implement cancellation
        int timer_id;
    };

    // Used by timer's wait initiation
    void add_timer(pending_timer&& t)
    {
        if (t.expiry <= current_time_)
        {
            // If the timer's expiry is in the past, directly call the handler
            post_handler(std::move(t), error_code());
        }
        else
        {
            // Add the timer op into the queue
            pending_.push_front(std::move(t));

            // Enable cancellation
            auto slot = asio::get_associated_cancellation_slot(pending_.front().handler);
            if (slot.is_connected())
            {
                slot.emplace<cancel_handler>(cancel_handler{this, pending_.begin()});
            }
        }
    }

    // Cancel all ops for the given timer_id
    std::size_t cancel(int timer_id)
    {
        std::size_t num_cancels = 0;
        for (auto it = pending_.begin(); it != pending_.end();)
        {
            if (it->timer_id == timer_id)
            {
                ++num_cancels;
                it = erase_and_post_handler(it, asio::error::operation_aborted);
            }
            else
            {
                ++it;
            }
        }
        return num_cancels;
    }

    // Set the new current time, calling handlers in the process
    void advance_time_to(std::chrono::steady_clock::time_point new_time)
    {
        for (auto it = pending_.begin(); it != pending_.end();)
        {
            if (it->expiry <= new_time)
                it = erase_and_post_handler(it, error_code());
            else
                ++it;
        }
        current_time_ = new_time;
    }

    // Same, but with a duration
    void advance_time_by(std::chrono::steady_clock::duration by) { advance_time_to(current_time_ + by); }

    // Used by timers, to retrieve their timer id
    int allocate_timer_id() { return ++current_timer_id_; }

    std::chrono::steady_clock::time_point current_time() const noexcept { return current_time_; }

private:
    std::list<pending_timer> pending_;
    std::chrono::steady_clock::time_point current_time_;
    int current_timer_id_{0};

    struct cancel_handler
    {
        mock_timer_service* svc;
        std::list<pending_timer>::iterator it;

        void operator()(asio::cancellation_type_t)
        {
            svc->erase_and_post_handler(it, asio::error::operation_aborted);
        }
    };

    // Schedule the handler to be called
    void post_handler(pending_timer&& t, error_code ec)
    {
        asio::get_associated_cancellation_slot(t.handler).clear();

        struct post_handler
        {
            asio::executor_work_guard<asio::any_io_executor> work;
            asio::any_completion_handler<void(error_code)> handler;
            error_code ec;

            void operator()()
            {
                work.reset();
                std::move(handler)(ec);
            }

            using executor_type = asio::any_completion_executor;
            executor_type get_executor() const { return asio::get_associated_executor(handler); }
        };

        asio::post(std::move(t.ex), post_handler{std::move(t.work_guard), std::move(t.handler), ec});
    }

    // Same, but also removes the op from the list
    std::list<pending_timer>::iterator erase_and_post_handler(
        std::list<pending_timer>::iterator it,
        error_code ec
    )
    {
        auto t = std::move(*it);
        auto res = pending_.erase(it);
        post_handler(std::move(t), ec);
        return res;
    }
};

// Helper
template <class ExecutionContext>
void advance_time_by(ExecutionContext& ctx, std::chrono::steady_clock::duration dur)
{
    asio::use_service<mock_timer_service>(ctx.get_executor().context()).advance_time_by(dur);
}

// A mock for asio::steady_timer
class mock_timer
{
    mock_timer_service* svc_;
    int timer_id_;
    asio::any_io_executor ex_;
    std::chrono::steady_clock::time_point expiry_;

    struct initiate_wait
    {
        template <class Handler>
        void operator()(Handler&& h, mock_timer* self)
        {
            // If the handler had an executor, use this. Otherwise, use the timer's
            auto ex = asio::get_associated_executor(h, self->ex_);
            self->svc_->add_timer(
                {self->expiry_, ex, asio::make_work_guard(ex), std::forward<Handler>(h), self->timer_id_}
            );
        }
    };

public:
    mock_timer(asio::any_io_executor ex)
        : svc_(&asio::use_service<mock_timer_service>(ex.context())),
          timer_id_(svc_->allocate_timer_id()),
          ex_(std::move(ex)),
          expiry_(svc_->current_time())
    {
    }

    mock_timer(asio::any_io_executor ex, std::chrono::steady_clock::time_point tp) : mock_timer(std::move(ex))
    {
        expires_at(tp);
    }

    asio::any_io_executor get_executor() { return ex_; }

    std::size_t expires_at(std::chrono::steady_clock::time_point new_expiry)
    {
        // cancel anything in flight, then set expiry
        std::size_t res = svc_->cancel(timer_id_);
        expiry_ = new_expiry;
        return res;
    }

    std::size_t expires_after(std::chrono::steady_clock::duration dur)
    {
        return expires_at(svc_->current_time() + dur);
    }

    std::size_t cancel() { return svc_->cancel(timer_id_); }

    template <class CompletionToken>
    auto async_wait(CompletionToken&& token)
        -> decltype(asio::async_initiate<CompletionToken, void(error_code)>(
            initiate_wait(),
            token,
            std::declval<mock_timer*>()
        ))
    {
        return asio::async_initiate<CompletionToken, void(error_code)>(initiate_wait(), token, this);
    }
};

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
