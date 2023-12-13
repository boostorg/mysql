//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/error_code.hpp>

#include <boost/mysql/impl/internal/connection_pool/wait_group.hpp>

#include <boost/asio/deferred.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/test/unit_test.hpp>

#include <chrono>

using namespace boost::mysql::detail;
namespace asio = boost::asio;
using boost::mysql::error_code;

BOOST_AUTO_TEST_SUITE(test_wait_group)

BOOST_AUTO_TEST_CASE(wait_add_remove)
{
    asio::io_context ctx;
    wait_group gp(ctx.get_executor());
    bool called = false;
    gp.async_wait([&](error_code) { called = true; });

    ctx.poll();
    BOOST_TEST(!called);

    gp.on_task_start();
    ctx.poll();
    BOOST_TEST(!called);

    gp.on_task_start();
    ctx.poll();
    BOOST_TEST(!called);

    gp.on_task_finish();
    ctx.poll();
    BOOST_TEST(!called);

    gp.on_task_finish();
    ctx.poll();
    BOOST_TEST(called);
}

BOOST_AUTO_TEST_CASE(wait_add_remove_add_remove)
{
    asio::io_context ctx;
    wait_group gp(ctx.get_executor());
    bool called = false;
    gp.async_wait([&](error_code) { called = true; });

    ctx.poll();
    BOOST_TEST(!called);

    gp.on_task_start();
    ctx.poll();
    BOOST_TEST(!called);

    gp.on_task_start();
    ctx.poll();
    BOOST_TEST(!called);

    gp.on_task_finish();
    ctx.poll();
    BOOST_TEST(!called);

    gp.on_task_start();
    ctx.poll();
    BOOST_TEST(!called);

    gp.on_task_finish();
    ctx.poll();
    BOOST_TEST(!called);

    gp.on_task_finish();
    ctx.poll();
    BOOST_TEST(called);
}

BOOST_AUTO_TEST_CASE(add_wait_remove)
{
    asio::io_context ctx;
    ctx.get_executor().on_work_started();
    wait_group gp(ctx.get_executor());
    bool called = false;

    gp.on_task_start();
    ctx.poll();
    BOOST_TEST(!called);

    gp.on_task_start();
    ctx.poll();
    BOOST_TEST(!called);

    gp.async_wait([&](error_code) { called = true; });
    ctx.poll();
    BOOST_TEST(!called);

    gp.on_task_finish();
    ctx.poll();
    BOOST_TEST(!called);

    gp.on_task_finish();
    ctx.poll();
    BOOST_TEST(called);
}

BOOST_AUTO_TEST_CASE(run_task)
{
    asio::io_context ctx;
    ctx.get_executor().on_work_started();
    asio::steady_timer timer1(ctx);
    asio::steady_timer timer2(ctx);
    timer1.expires_at(std::chrono::steady_clock::time_point::max());
    timer2.expires_at(std::chrono::steady_clock::time_point::max());
    wait_group gp(ctx.get_executor());

    bool called = false;

    gp.async_wait([&](error_code) { called = true; });
    ctx.poll();
    BOOST_TEST(!called);

    gp.run_task(timer1.async_wait(asio::deferred));
    ctx.poll();
    BOOST_TEST(!called);

    gp.run_task(timer2.async_wait(asio::deferred));
    ctx.poll();
    BOOST_TEST(!called);

    timer2.cancel();
    ctx.poll();
    BOOST_TEST(!called);

    timer1.cancel();
    ctx.poll();
    BOOST_TEST(called);
}

BOOST_AUTO_TEST_SUITE_END()