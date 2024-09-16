//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/any_completion_executor.hpp>
#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/associated_cancellation_slot.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/execution_context.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/post.hpp>
#include <boost/system/error_code.hpp>

#include <chrono>
#include <cstddef>
#include <list>
#include <utility>

#include "test_unit/mock_timer.hpp"

namespace asio = boost::asio;
namespace mysql = boost::mysql;
using boost::mysql::test::mock_clock;
using boost::system::error_code;
using std::chrono::steady_clock;

// The current time
static thread_local steady_clock::time_point g_mock_now{};

/**
 * Used by mock timers. Like asio::detail::deadline_timer_service, but for mock timers.
 * Mock timers don't rely on the actual clock, but on a time_point hold by this class.
 * Call mock_timer_service::advance_time_xx to adjust the current time. This will
 * call timer handlers as if time had advanced. Note that we don't have a way to mock
 * steady_clock::now(). Our code under test must make sure not to call it.
 */
class boost::mysql::test::mock_timer_service : public asio::execution_context::service
{
public:
    // Required by all Boost.Asio services
    mock_timer_service(asio::execution_context& owner) : asio::execution_context::service(owner) {}

    static const asio::execution_context::id id;

    void shutdown() final
    {
        // Cancel all operations. Operations may allocate I/O objects for other services,
        // which must be destroyed before services are actually destroyed.
        while (!pending_.empty())
            erase_and_post_handler(pending_.begin(), asio::error::operation_aborted);
    }

    // A pending timer
    struct pending_timer
    {
        // When does the timer expire?
        steady_clock::time_point expiry;

        // The timer's executor work guard
        asio::executor_work_guard<asio::any_io_executor> timer_work;

        // The handler's executor work guard
        asio::executor_work_guard<asio::any_completion_executor> handler_work;

        // What handler should we call?
        asio::any_completion_handler<void(error_code)> handler;

        // Uniquely identifies the timer, so we can implement cancellation
        int timer_id;
    };

    // Used by timer's wait initiation
    void add_timer(pending_timer&& t)
    {
        if (t.expiry <= current_time())
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
    void advance_time_to(steady_clock::time_point new_time)
    {
        for (auto it = pending_.begin(); it != pending_.end();)
        {
            if (it->expiry <= new_time)
                it = erase_and_post_handler(it, error_code());
            else
                ++it;
        }
        set_current_time(new_time);
    }

    // Same, but with a duration
    void advance_time_by(steady_clock::duration by) { advance_time_to(current_time() + by); }

    // Used by timers, to retrieve their timer id
    int allocate_timer_id() { return ++current_timer_id_; }

    steady_clock::time_point current_time() const { return g_mock_now; }

private:
    std::list<pending_timer> pending_;
    int current_timer_id_{0};

    void set_current_time(steady_clock::time_point to) { g_mock_now = to; }

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
            asio::executor_work_guard<asio::any_io_executor> timer_work;
            asio::executor_work_guard<asio::any_completion_executor> handler_work;
            asio::any_completion_handler<void(error_code)> handler;
            error_code ec;

            void operator()()
            {
                timer_work.reset();
                handler_work.reset();
                std::move(handler)(ec);
            }

            using executor_type = asio::any_completion_executor;
            executor_type get_executor() const
            {
                return asio::get_associated_executor(handler, timer_work.get_executor());
            }
        };

        auto timer_ex = t.timer_work.get_executor();
        asio::post(
            std::move(timer_ex),
            post_handler{std::move(t.timer_work), std::move(t.handler_work), std::move(t.handler), ec}
        );
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

const asio::execution_context::id boost::mysql::test::mock_timer_service::id;

mock_clock::time_point boost::mysql::test::mock_clock::now() { return g_mock_now; }

void boost::mysql::test::advance_time_by(asio::execution_context& ctx, steady_clock::duration dur)
{
    asio::use_service<mock_timer_service>(ctx).advance_time_by(dur);
}

void boost::asio::basic_waitable_timer<mock_clock>::add_to_service(
    asio::any_completion_handler<void(system::error_code)> handler
)
{
    auto handler_ex = asio::any_completion_executor(asio::get_associated_executor(handler));
    svc_->add_timer({
        expiry_,
        asio::make_work_guard(ex_),
        asio::make_work_guard(std::move(handler_ex)),
        std::move(handler),
        timer_id_,
    });
}

boost::asio::basic_waitable_timer<mock_clock>::basic_waitable_timer(asio::any_io_executor ex)
    : svc_(&asio::use_service<mysql::test::mock_timer_service>(ex.context())),
      timer_id_(svc_->allocate_timer_id()),
      ex_(std::move(ex)),
      expiry_(svc_->current_time())
{
}

std::size_t boost::asio::basic_waitable_timer<mock_clock>::expires_at(steady_clock::time_point new_expiry)
{
    // cancel anything in flight, then set expiry
    std::size_t res = svc_->cancel(timer_id_);
    expiry_ = new_expiry;
    return res;
}

std::size_t boost::asio::basic_waitable_timer<mock_clock>::expires_after(steady_clock::duration dur)
{
    return expires_at(svc_->current_time() + dur);
}

std::size_t boost::asio::basic_waitable_timer<mock_clock>::cancel() { return svc_->cancel(timer_id_); }