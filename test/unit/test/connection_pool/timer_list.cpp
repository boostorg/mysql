//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/impl/internal/connection_pool/timer_list.hpp>

#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_suite.hpp>

#include <chrono>
#include <memory>

using namespace boost::mysql::detail;
namespace asio = boost::asio;

BOOST_AUTO_TEST_SUITE(test_timer_list)

// We use real timers to verify that cancel semantics integrate
// correctly with our code
using list_t = timer_list<asio::steady_timer>;
using block_t = timer_block<asio::steady_timer>;

struct fixture
{
    list_t l;
    asio::io_context ctx;

    static void add_wait(block_t& blk)
    {
        blk.timer.expires_after(std::chrono::minutes(30));
        blk.timer.async_wait(asio::detached);
    }
};

BOOST_FIXTURE_TEST_CASE(notify_one_empty, fixture)
{
    BOOST_TEST(l.size() == 0u);

    // notify_one doesn't crash if the list is empty
    BOOST_CHECK_NO_THROW(l.notify_one());
}

BOOST_FIXTURE_TEST_CASE(notify_one_several_timers, fixture)
{
    // Create timers
    block_t t1(ctx.get_executor());
    block_t t2(ctx.get_executor());

    // Add waits on them
    add_wait(t1);
    add_wait(t2);

    // Add them to the list
    l.push_back(t1);
    l.push_back(t2);
    BOOST_TEST(l.size() == 2u);

    // Notify causes the first one to be cancelled
    l.notify_one();
    BOOST_TEST(t1.timer.cancel() == 0u);
    BOOST_TEST(t2.timer.cancel() == 1u);
}

BOOST_FIXTURE_TEST_CASE(notify_one_timer_multiple_waits, fixture)
{
    // Create timers
    block_t t1(ctx.get_executor());
    block_t t2(ctx.get_executor());

    // Add multiple waits to the first one
    add_wait(t1);
    add_wait(t2);
    t1.timer.async_wait(asio::detached);

    // Add them to the list
    l.push_back(t1);
    l.push_back(t2);
    BOOST_TEST(l.size() == 2u);

    // Notify causes all waits on the first one to be cancelled
    l.notify_one();
    BOOST_TEST(t1.timer.cancel() == 0u);
    BOOST_TEST(t2.timer.cancel() == 1u);
}

BOOST_FIXTURE_TEST_CASE(notify_one_timer_already_cancelled, fixture)
{
    // Create timers
    block_t t1(ctx.get_executor());
    block_t t2(ctx.get_executor());
    block_t t3(ctx.get_executor());

    // Add waits on them
    add_wait(t1);
    add_wait(t2);
    add_wait(t3);

    // Add them to the list
    l.push_back(t1);
    l.push_back(t2);
    l.push_back(t3);
    BOOST_TEST(l.size() == 3u);

    // The first notify cancels the first timer, the second notify, the second timer
    l.notify_one();
    l.notify_one();
    BOOST_TEST(t1.timer.cancel() == 0u);
    BOOST_TEST(t2.timer.cancel() == 0u);
    BOOST_TEST(t3.timer.cancel() == 1u);
}

BOOST_FIXTURE_TEST_CASE(notify_one_all_timers_cancelled, fixture)
{
    // Create timers
    block_t t1(ctx.get_executor());
    block_t t2(ctx.get_executor());

    // Add waits on them
    add_wait(t1);
    add_wait(t2);

    // Add them to the list
    l.push_back(t1);
    l.push_back(t2);
    BOOST_TEST(l.size() == 2u);

    // The third call will find all timers already cancelled
    l.notify_one();
    l.notify_one();
    l.notify_one();
    BOOST_TEST(t1.timer.cancel() == 0u);
    BOOST_TEST(t2.timer.cancel() == 0u);
}

BOOST_FIXTURE_TEST_CASE(notify_one_timer_without_wait, fixture)
{
    // Create timers
    block_t t1(ctx.get_executor());
    block_t t2(ctx.get_executor());
    block_t t3(ctx.get_executor());

    // Add waits on t2 and t3, but not t1. This can happen
    // if we insert the timer on the list before adding a wait
    add_wait(t2);
    add_wait(t3);

    // Insert them on the list
    l.push_back(t1);
    l.push_back(t2);
    l.push_back(t3);
    BOOST_TEST(l.size() == 3u);

    // Since there's nothing to notify on t1, we notify t2
    l.notify_one();
    BOOST_TEST(t1.timer.cancel() == 0u);
    BOOST_TEST(t2.timer.cancel() == 0u);
    BOOST_TEST(t3.timer.cancel() == 1u);
}

BOOST_FIXTURE_TEST_CASE(notify_all_empty, fixture)
{
    // notify_all doesn't crash if the list is empty
    BOOST_CHECK_NO_THROW(l.notify_all());
}

BOOST_FIXTURE_TEST_CASE(notify_all_some_timers, fixture)
{
    // Create timers
    block_t t1(ctx.get_executor());
    block_t t2(ctx.get_executor());

    // Add waits on them
    add_wait(t1);
    add_wait(t2);

    // Add them to the list
    l.push_back(t1);
    l.push_back(t2);
    BOOST_TEST(l.size() == 2u);

    // Notify cancels all timers
    l.notify_all();
    BOOST_TEST(t1.timer.cancel() == 0u);
    BOOST_TEST(t2.timer.cancel() == 0u);

    // Notifying again is a no-op
    l.notify_all();
    BOOST_TEST(t1.timer.cancel() == 0u);
    BOOST_TEST(t2.timer.cancel() == 0u);
}

BOOST_FIXTURE_TEST_CASE(auto_unlink, fixture)
{
    // Create timers (in dynamic memory so we can delete them easily)
    std::unique_ptr<block_t> t1{new block_t(ctx.get_executor())};
    std::unique_ptr<block_t> t2{new block_t(ctx.get_executor())};
    std::unique_ptr<block_t> t3{new block_t(ctx.get_executor())};

    // Add waits on some of them
    add_wait(*t2);
    add_wait(*t3);

    // Add them to the list
    l.push_back(*t1);
    l.push_back(*t2);
    l.push_back(*t3);
    BOOST_TEST(l.size() == 3u);

    // Delete t2
    t2.reset();
    BOOST_TEST(l.size() == 2u);

    // Delete t1
    t1.reset();
    BOOST_TEST(l.size() == 1u);

    // notify_one doesn't try to notify deleted timers
    l.notify_one();
    BOOST_TEST(t3->timer.cancel() == 0u);

    // Delete the last one
    t3.reset();
    BOOST_TEST(l.size() == 0u);
}

BOOST_AUTO_TEST_SUITE_END()
