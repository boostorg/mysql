// header

#include <boost/mysql/impl/internal/connection_pool/timer_list.hpp>

#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/test/unit_test.hpp>

#include <chrono>
#include <memory>

using namespace boost::mysql::detail;
namespace asio = boost::asio;

BOOST_AUTO_TEST_SUITE(test_timer_list)

// We use real timers to verify that cancel semantics integrate
// correctly with our code
using list_t = timer_list<asio::steady_timer>;
using block_t = timer_block<asio::steady_timer>;

std::unique_ptr<block_t> create_block(asio::io_context& ctx)
{
    return std::unique_ptr<block_t>{new block_t(ctx.get_executor())};
}

BOOST_AUTO_TEST_CASE(notify_one_empty)
{
    list_t l;
    BOOST_TEST(l.size() == 0u);
    BOOST_CHECK_NO_THROW(l.notify_one());
}

BOOST_AUTO_TEST_CASE(notify_one_several_timers)
{
    list_t l;
    asio::io_context ctx;

    auto t1 = create_block(ctx);
    auto t2 = create_block(ctx);
    t1->timer.expires_after(std::chrono::minutes(20));
    t2->timer.expires_after(std::chrono::minutes(30));
    t1->timer.async_wait(asio::detached);
    t2->timer.async_wait(asio::detached);

    l.push_back(*t1);
    l.push_back(*t2);

    BOOST_TEST(l.size() == 2u);

    l.notify_one();

    BOOST_TEST(t1->timer.cancel() == 0u);
    BOOST_TEST(t2->timer.cancel() == 1u);
}

BOOST_AUTO_TEST_CASE(notify_one_timer_multiple_waits)
{
    list_t l;
    asio::io_context ctx;

    auto t1 = create_block(ctx);
    auto t2 = create_block(ctx);
    t1->timer.expires_after(std::chrono::minutes(20));
    t2->timer.expires_after(std::chrono::minutes(30));
    t1->timer.async_wait(asio::detached);
    t1->timer.async_wait(asio::detached);
    t2->timer.async_wait(asio::detached);

    l.push_back(*t1);
    l.push_back(*t2);

    BOOST_TEST(l.size() == 2u);

    l.notify_one();

    BOOST_TEST(t1->timer.cancel() == 0u);
    BOOST_TEST(t2->timer.cancel() == 1u);
}

BOOST_AUTO_TEST_CASE(notify_one_timer_already_cancelled)
{
    list_t l;
    asio::io_context ctx;

    auto t1 = create_block(ctx);
    auto t2 = create_block(ctx);
    auto t3 = create_block(ctx);
    t1->timer.expires_after(std::chrono::minutes(20));
    t2->timer.expires_after(std::chrono::minutes(30));
    t3->timer.expires_after(std::chrono::minutes(40));
    t1->timer.async_wait(asio::detached);
    t2->timer.async_wait(asio::detached);
    t3->timer.async_wait(asio::detached);

    l.push_back(*t1);
    l.push_back(*t2);
    l.push_back(*t3);

    BOOST_TEST(l.size() == 3u);

    l.notify_one();
    l.notify_one();

    BOOST_TEST(t1->timer.cancel() == 0u);
    BOOST_TEST(t2->timer.cancel() == 0u);
    BOOST_TEST(t3->timer.cancel() == 1u);
}

BOOST_AUTO_TEST_CASE(notify_one_all_timers_cancelled)
{
    list_t l;
    asio::io_context ctx;

    auto t1 = create_block(ctx);
    auto t2 = create_block(ctx);
    t1->timer.expires_after(std::chrono::minutes(20));
    t2->timer.expires_after(std::chrono::minutes(30));
    t1->timer.async_wait(asio::detached);
    t2->timer.async_wait(asio::detached);

    l.push_back(*t1);
    l.push_back(*t2);

    BOOST_TEST(l.size() == 2u);

    l.notify_one();
    l.notify_one();
    l.notify_one();

    BOOST_TEST(t1->timer.cancel() == 0u);
    BOOST_TEST(t2->timer.cancel() == 0u);
}

BOOST_AUTO_TEST_CASE(notify_one_timer_expired)
{
    list_t l;
    asio::io_context ctx;

    auto t1 = create_block(ctx);
    auto t2 = create_block(ctx);
    auto t3 = create_block(ctx);
    t2->timer.expires_after(std::chrono::minutes(30));
    t3->timer.expires_after(std::chrono::minutes(40));
    t1->timer.async_wait(asio::detached);
    t2->timer.async_wait(asio::detached);
    t3->timer.async_wait(asio::detached);

    l.push_back(*t1);
    l.push_back(*t2);
    l.push_back(*t3);

    BOOST_TEST(l.size() == 3u);

    ctx.run_one();

    l.notify_one();

    BOOST_TEST(t1->timer.cancel() == 0u);
    BOOST_TEST(t2->timer.cancel() == 0u);
    BOOST_TEST(t3->timer.cancel() == 1u);
}

BOOST_AUTO_TEST_CASE(notify_one_timer_without_wait)
{
    list_t l;
    asio::io_context ctx;

    auto t1 = create_block(ctx);
    auto t2 = create_block(ctx);
    auto t3 = create_block(ctx);
    t2->timer.expires_after(std::chrono::minutes(30));
    t3->timer.expires_after(std::chrono::minutes(40));
    t2->timer.async_wait(asio::detached);
    t3->timer.async_wait(asio::detached);

    l.push_back(*t1);
    l.push_back(*t2);
    l.push_back(*t3);

    BOOST_TEST(l.size() == 3u);

    l.notify_one();

    BOOST_TEST(t1->timer.cancel() == 0u);
    BOOST_TEST(t2->timer.cancel() == 0u);
    BOOST_TEST(t3->timer.cancel() == 1u);
}

BOOST_AUTO_TEST_CASE(notify_all_empty)
{
    list_t l;
    BOOST_CHECK_NO_THROW(l.notify_all());
}

BOOST_AUTO_TEST_CASE(notify_all_some_timers)
{
    list_t l;
    asio::io_context ctx;

    auto t1 = create_block(ctx);
    auto t2 = create_block(ctx);
    t1->timer.expires_after(std::chrono::minutes(20));
    t2->timer.expires_after(std::chrono::minutes(30));
    t1->timer.async_wait(asio::detached);
    t2->timer.async_wait(asio::detached);

    l.push_back(*t1);
    l.push_back(*t2);

    BOOST_TEST(l.size() == 2u);

    l.notify_all();
    l.notify_all();

    BOOST_TEST(t1->timer.cancel() == 0u);
    BOOST_TEST(t2->timer.cancel() == 0u);
}

BOOST_AUTO_TEST_CASE(auto_unlink)
{
    list_t l;
    asio::io_context ctx;

    auto t1 = create_block(ctx);
    auto t2 = create_block(ctx);
    auto t3 = create_block(ctx);
    t2->timer.expires_after(std::chrono::minutes(30));
    t3->timer.expires_after(std::chrono::minutes(40));
    t2->timer.async_wait(asio::detached);
    t3->timer.async_wait(asio::detached);

    l.push_back(*t1);
    l.push_back(*t2);
    l.push_back(*t3);

    BOOST_TEST(l.size() == 3u);

    t2.reset();
    BOOST_TEST(l.size() == 2u);

    t1.reset();
    BOOST_TEST(l.size() == 1u);

    l.notify_one();
    BOOST_TEST(t3->timer.cancel() == 0u);

    t3.reset();
    BOOST_TEST(l.size() == 0u);
}

BOOST_AUTO_TEST_SUITE_END()
