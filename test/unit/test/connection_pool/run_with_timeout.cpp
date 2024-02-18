//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/impl/internal/connection_pool/run_with_timeout.hpp>

#include <boost/asio/any_completion_executor.hpp>
#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/associated_cancellation_slot.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/bind_allocator.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/optional/optional.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_suite.hpp>

#include <chrono>
#include <utility>

#include "mock_timer.hpp"
#include "test_common/printing.hpp"

#ifdef __cpp_lib_polymorphic_allocator
#include <memory_resource>
#endif

using namespace boost::mysql;
namespace asio = boost::asio;
using detail::run_with_timeout;
using std::chrono::seconds;

BOOST_AUTO_TEST_SUITE(test_run_with_timeout)

// An I/O object mock. An operation is started using async_f,
// and it will be outstanding until manually completed with complete()
// or cancelled. We call handlers directly (instead of using dispatch)
// because all tests always run in an I/O context thread.
class mock_io_obj
{
    // Handler to call
    asio::any_completion_handler<void(error_code)> h_;

    // Keep work active while we have a handler. Guards are not assignable,
    // so we need this workaround
    boost::optional<asio::executor_work_guard<asio::any_completion_executor>> work_;

    struct cancel_handler
    {
        mock_io_obj* self;

        void operator()(asio::cancellation_type) const
        {
            std::move(self->h_)(client_errc::auth_plugin_requires_ssl);
        }
    };

    void set_handler(asio::any_completion_handler<void(error_code)> h)
    {
        h_ = std::move(h);
        work_.emplace(asio::make_work_guard(asio::get_associated_executor(h)));
        auto slot = asio::get_associated_cancellation_slot(h_);
        if (slot.is_connected())
        {
            slot.template emplace<cancel_handler>(cancel_handler{this});
        }
    }

    struct initiate_f
    {
        template <class Handler>
        void operator()(Handler&& h, mock_io_obj* self)
        {
            self->set_handler(std::move(h));
        }
    };

public:
    template <class CompletionToken>
    auto async_f(CompletionToken&& token) -> decltype(asio::async_initiate<CompletionToken, void(error_code)>(
        initiate_f{},
        token,
        std::declval<mock_io_obj*>()
    ))
    {
        return asio::async_initiate<CompletionToken, void(error_code)>(initiate_f{}, token, this);
    }

    // There must be a handler set up before calling complete().
    // This also verifies that our operation is being run in the expected executor.
    void complete(error_code ec, asio::any_completion_executor expected_ex)
    {
        BOOST_ASSERT(h_);
        BOOST_TEST((asio::get_associated_executor(h_) == expected_ex));
        asio::get_associated_cancellation_slot(h_).clear();
        work_.reset();
        std::move(h_)(ec);
    }
};

struct fixture
{
    // Context and objects
    asio::io_context ctx;
    mock_io_obj io;
    test::mock_timer tim{ctx.get_executor()};

    // Ensure the operation finished
    bool finished{false};

    void set_finished() noexcept { finished = true; }

    ~fixture() { BOOST_TEST(finished); }
};

// The operation finishes first and successfully
BOOST_FIXTURE_TEST_CASE(op_first_ok, fixture)
{
    // Run the op
    run_with_timeout(io.async_f(asio::deferred), tim, seconds(60), [this](error_code ec) {
        BOOST_TEST(ec == error_code());
        set_finished();
    });

    // Complete
    io.complete(error_code(), ctx.get_executor());
    ctx.poll();
}

// The operation finishes first with an error
BOOST_FIXTURE_TEST_CASE(op_first_error, fixture)
{
    // Run the op
    run_with_timeout(io.async_f(asio::deferred), tim, seconds(60), [this](error_code ec) {
        BOOST_TEST(ec == client_errc::extra_bytes);
        set_finished();
    });

    // Complete with an error
    io.complete(client_errc::extra_bytes, ctx.get_executor());
    ctx.poll();
}

// The operation finishes first and at the same time than the timer
BOOST_FIXTURE_TEST_CASE(op_first_timer_ok, fixture)
{
    // Run the op
    run_with_timeout(io.async_f(asio::deferred), tim, seconds(60), [this](error_code ec) {
        BOOST_TEST(ec == error_code());
        set_finished();
    });

    // Time elapses
    test::advance_time_by(ctx, seconds(60));

    // Operation completes successfully
    io.complete(error_code(), ctx.get_executor());
    ctx.poll();
}

// The timer finishes first without an error (timeout)
BOOST_FIXTURE_TEST_CASE(timer_first_ok, fixture)
{
    // Run the op
    run_with_timeout(io.async_f(asio::deferred), tim, seconds(60), [this](error_code ec) {
        BOOST_TEST(ec == client_errc::timeout);
        set_finished();
    });

    // Advance time
    test::advance_time_by(ctx, seconds(60));
    ctx.poll();
}

// The timer finishes first because it was cancelled
BOOST_FIXTURE_TEST_CASE(timer_first_cancelled, fixture)
{
    // Run the op
    run_with_timeout(io.async_f(asio::deferred), tim, seconds(60), [this](error_code ec) {
        BOOST_TEST(ec == client_errc::cancelled);
        set_finished();
    });

    // Cancel the timer
    tim.cancel();
    ctx.poll();
}

// Zero timeout disables it
BOOST_FIXTURE_TEST_CASE(timeout_zero, fixture)
{
    // Run the op
    run_with_timeout(io.async_f(asio::deferred), tim, seconds(0), [this](error_code ec) {
        BOOST_TEST(ec == error_code());
        set_finished();
    });

    // Advancing time does nothing
    test::advance_time_by(ctx, seconds(60));
    ctx.poll();
    BOOST_TEST(!finished);

    // Complete
    io.complete(error_code(), ctx.get_executor());
    ctx.poll();
}

// We release any allocated memory before calling the final handler
#ifdef __cpp_lib_polymorphic_allocator
BOOST_FIXTURE_TEST_CASE(memory_released_before_calling_handler, fixture)
{
    // A memory resource that tracks allocations and deallocations
    struct tracking_resource final : std::pmr::memory_resource
    {
        int num_allocs{0}, num_deallocs{0};

        void* do_allocate(std::size_t bytes, std::size_t alignment) override
        {
            ++num_allocs;
            return std::pmr::get_default_resource()->allocate(bytes, alignment);
        }

        void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) override
        {
            ++num_deallocs;
            return std::pmr::get_default_resource()->deallocate(p, bytes, alignment);
        }

        bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override
        {
            return this == &other;
        }
    } resource;

    // The completion handler. We don't use bind_allocator because of this bug
    // https://github.com/chriskohlhoff/asio/issues/1429
    struct handler
    {
        tracking_resource* res;
        fixture* fix;

        using allocator_type = std::pmr::polymorphic_allocator<void>;
        allocator_type get_allocator() const noexcept { return allocator_type(res); }

        void operator()(error_code ec)
        {
            BOOST_TEST(ec == error_code());
            BOOST_TEST(res->num_allocs == res->num_deallocs);
            fix->set_finished();
        }
    };

    // Run the op and complete
    run_with_timeout(io.async_f(asio::deferred), tim, seconds(60), handler{&resource, this});
    io.complete(error_code(), ctx.get_executor());
    ctx.poll();
}
#endif

BOOST_AUTO_TEST_SUITE_END()
