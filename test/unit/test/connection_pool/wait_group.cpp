//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/error_code.hpp>

#include <boost/mysql/impl/internal/connection_pool/wait_group.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/execution/outstanding_work.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/test/unit_test.hpp>

#include <chrono>

using namespace boost::mysql::detail;
namespace asio = boost::asio;
using boost::mysql::error_code;

BOOST_AUTO_TEST_SUITE(test_wait_group)

struct fixture
{
    asio::io_context ctx;
    wait_group gp{ctx.get_executor()};
    bool called{false};
    asio::any_io_executor work_guard;

    // If there is no work present and we call ctx.poll(),
    // the context is stopped and won't process further handlers
    fixture() : work_guard(asio::require(ctx.get_executor(), asio::execution::outstanding_work.tracked)) {}

    // We call ctx.poll() after every action so any pending handler is dispatched
    void launch_wait()
    {
        gp.async_wait([&](error_code) { called = true; });
        ctx.poll();
    }

    void call_task_start()
    {
        gp.on_task_start();
        ctx.poll();
    }

    void call_task_finish()
    {
        gp.on_task_finish();
        ctx.poll();
    }
};

BOOST_FIXTURE_TEST_CASE(wait_add_remove, fixture)
{
    // Launching the wait won't call the handler even if no task has started yet
    launch_wait();
    BOOST_TEST(!called);

    // Launch two tasks
    call_task_start();
    BOOST_TEST(!called);
    call_task_start();
    BOOST_TEST(!called);

    // Finish them
    call_task_finish();
    BOOST_TEST(!called);
    call_task_finish();
    BOOST_TEST(called);
}

BOOST_FIXTURE_TEST_CASE(wait_add_remove_add_remove, fixture)
{
    // Launching the wait won't call the handler even if no task has started yet
    launch_wait();
    BOOST_TEST(!called);

    // Launch two tasks
    call_task_start();
    BOOST_TEST(!called);
    call_task_start();
    BOOST_TEST(!called);

    // Finish one
    call_task_finish();
    BOOST_TEST(!called);

    // Start another
    call_task_start();
    BOOST_TEST(!called);

    // Finish remaining tasks
    call_task_finish();
    BOOST_TEST(!called);
    call_task_finish();
    BOOST_TEST(called);
}

BOOST_FIXTURE_TEST_CASE(add_wait_remove, fixture)
{
    // Start two tasks
    call_task_start();
    BOOST_TEST(!called);
    call_task_start();
    BOOST_TEST(!called);

    // Start the wait
    launch_wait();
    BOOST_TEST(!called);

    // Finish the two tasks
    call_task_finish();
    BOOST_TEST(!called);
    call_task_finish();
    BOOST_TEST(called);
}

BOOST_FIXTURE_TEST_CASE(run_task, fixture)
{
    // Create two timers to simulate async operations
    asio::steady_timer timer1(ctx);
    asio::steady_timer timer2(ctx);
    timer1.expires_at((std::chrono::steady_clock::time_point::max)());
    timer2.expires_at((std::chrono::steady_clock::time_point::max)());

    // Start the wait
    launch_wait();
    BOOST_TEST(!called);

    // Launch the two async ops
    gp.run_task(timer1.async_wait(asio::deferred));
    ctx.poll();
    BOOST_TEST(!called);

    gp.run_task(timer2.async_wait(asio::deferred));
    ctx.poll();
    BOOST_TEST(!called);

    // Finish one task
    timer2.cancel();
    ctx.poll();
    BOOST_TEST(!called);

    // Finish the other task
    timer1.cancel();
    ctx.poll();
    BOOST_TEST(called);
}

BOOST_AUTO_TEST_SUITE_END()