//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/connection_pool.hpp>
#include <boost/mysql/pool_params.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/ssl_mode.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <exception>
#include <iostream>

#ifdef BOOST_ASIO_HAS_CO_AWAIT

namespace mysql = boost::mysql;
namespace asio = boost::asio;

namespace {

static constexpr int num_parallel = 100;
static constexpr int total = num_parallel * 20;

class coordinator
{
    std::atomic_int remaining_queries_{total};
    std::atomic_int outstanding_tasks_{num_parallel};
    mysql::connection_pool* pool_{};

public:
    coordinator(mysql::connection_pool& pool) : pool_(&pool) {}
    void on_finish()
    {
        if (--outstanding_tasks_ == 0)
            pool_->cancel();
    }
    bool on_loop_finish() { return --remaining_queries_ > 0; }
};

asio::awaitable<void> task(mysql::connection_pool& pool, coordinator& coord)
{
    mysql::results r;

    // async_get_connection fails immediately if the pool is not running.
    // Grant it a small period of time to bootstrap
    asio::steady_timer timer(co_await asio::this_coro::executor);
    timer.expires_after(std::chrono::milliseconds(1));
    co_await timer.async_wait();

    while (true)
    {
        // Coroutine handlers always have a bound executor.
        // Verify that we achieve thread-safety even in this case (regression check)
        // Exercise return_without_reset, too
        auto conn = co_await pool.async_get_connection();
        co_await conn->async_execute("SELECT 1", r);
        conn.return_without_reset();

        if (!coord.on_loop_finish())
        {
            coord.on_finish();
            co_return;
        }
    }
}

void rethrow_on_err(std::exception_ptr err)
{
    if (err)
        std::rethrow_exception(err);
}

void run(const char* hostname)
{
    // Setup
    asio::thread_pool ctx(8);
    mysql::pool_params params;
    params.server_address.emplace_host_and_port(hostname);
    params.username = "integ_user";
    params.password = "integ_password";
    params.max_size = num_parallel - 10;    // create some contention
    params.ssl = mysql::ssl_mode::require;  // double check sharing SSL contexts is OK
    params.thread_safe = true;

    mysql::connection_pool pool(ctx, std::move(params));
    coordinator coord(pool);

    // The pool should be thread-safe even if we pass a token with a custom
    // executor to async_run (as happens with coroutines)
    asio::co_spawn(
        ctx,
        [&]() -> asio::awaitable<void> { return pool.async_run(asio::use_awaitable); },
        rethrow_on_err
    );

    // Create and launch tasks
    for (std::size_t i = 0; i < num_parallel; ++i)
    {
        asio::co_spawn(ctx, [&]() -> asio::awaitable<void> { return task(pool, coord); }, rethrow_on_err);
    }

    // Run
    ctx.join();
}

void usage(const char* progname)
{
    std::cerr << "Usage: " << progname << " <hostname>\n";
    exit(1);
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        usage(argv[0]);
    }

    run(argv[1]);
}

#else

int main() { std::cout << "Sorry, your compiler doesn't support C++20 coroutines\n"; }

#endif
