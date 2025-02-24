//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/connection_pool.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/pool_params.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/with_diagnostics.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancel_after.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/test/unit_test.hpp>

#include <chrono>

#include "test_common/ci_server.hpp"
#include "test_integration/run_coro.hpp"
#include "test_integration/snippets/credentials.hpp"

namespace asio = boost::asio;
namespace mysql = boost::mysql;
using namespace mysql::test;

namespace {

#ifdef BOOST_ASIO_HAS_CO_AWAIT
//[connection_pool_get_connection
// Use connection pools for functions that will be called
// repeatedly during the application lifetime.
// An HTTP server handler function is a good candidate.
asio::awaitable<std::int64_t> get_num_employees(mysql::connection_pool& pool)
{
    // Get a fresh connection from the pool.
    // pooled_connection is a proxy to an any_connection object.
    mysql::pooled_connection conn = co_await pool.async_get_connection();

    // Use pooled_connection::operator-> to access the underlying any_connection.
    // Let's use the connection
    mysql::results result;
    co_await conn->async_execute("SELECT COUNT(*) FROM employee", result);
    co_return result.rows().at(0).at(0).as_int64();

    // When conn is destroyed, the connection is returned to the pool
}
//]

asio::awaitable<void> return_without_reset(mysql::connection_pool& pool)
{
    //[connection_pool_return_without_reset
    // Get a connection from the pool
    mysql::pooled_connection conn = co_await pool.async_get_connection();

    // Use the connection in a way that doesn't mutate session state.
    // We're not setting variables, preparing statements or starting transactions,
    // so it's safe to skip reset
    mysql::results result;
    co_await conn->async_execute("SELECT COUNT(*) FROM employee", result);

    // Explicitly return the connection to the pool, skipping reset
    conn.return_without_reset();
    //]
}

asio::awaitable<void> apply_timeout(mysql::connection_pool& pool)
{
    //[connection_pool_apply_timeout
    // Get a connection from the pool, but don't wait more than 5 seconds
    auto conn = co_await pool.async_get_connection(asio::cancel_after(std::chrono::seconds(5)));
    //]

    conn.return_without_reset();
}
#endif

BOOST_AUTO_TEST_CASE(section_connection_pool)
{
    auto server_hostname = get_hostname();

    {
        //[connection_pool_create
        // pool_params contains configuration for the pool.
        // You must specify enough information to establish a connection,
        // including the server address and credentials.
        // You can configure a lot of other things, like pool limits
        mysql::pool_params params;
        params.server_address.emplace_host_and_port(server_hostname);
        params.username = mysql_username;
        params.password = mysql_password;
        params.database = "boost_mysql_examples";

        // The I/O context, required by all I/O operations
        asio::io_context ctx;

        // Construct a pool of connections. The context will be used internally
        // to create the connections and other I/O objects
        mysql::connection_pool pool(ctx, std::move(params));

        // You need to call async_run on the pool before doing anything useful with it.
        // async_run creates connections and keeps them healthy. It must be called
        // only once per pool.
        // The detached completion token means that we don't want to be notified when
        // the operation ends. It's similar to a no-op callback.
        pool.async_run(asio::detached);
        //]

#ifdef BOOST_ASIO_HAS_CO_AWAIT
        run_coro(ctx, [&pool]() -> asio::awaitable<void> {
            co_await get_num_employees(pool);
            pool.cancel();
        });
#endif
    }
    {
        asio::io_context ctx;

        //[connection_pool_configure_size
        mysql::pool_params params;

        // Set the usual params
        params.server_address.emplace_host_and_port(server_hostname);
        params.username = mysql_username;
        params.password = mysql_password;
        params.database = "boost_mysql_examples";

        // Create 10 connections at startup, and allow up to 1000 connections
        params.initial_size = 10;
        params.max_size = 1000;

        mysql::connection_pool pool(ctx, std::move(params));
        //]

#ifdef BOOST_ASIO_HAS_CO_AWAIT
        pool.async_run(asio::detached);
        run_coro(ctx, [&pool]() -> asio::awaitable<void> {
            co_await return_without_reset(pool);
            co_await apply_timeout(pool);
            pool.cancel();
        });
#endif
    }
    {
        //[connection_pool_thread_safe
        // The I/O context, required by all I/O operations
        asio::io_context ctx;

        // The usual pool configuration params
        mysql::pool_params params;
        params.server_address.emplace_host_and_port(server_hostname);
        params.username = mysql_username;
        params.password = mysql_password;
        params.database = "boost_mysql_examples";
        params.thread_safe = true;  // enable thread safety

        // Construct a thread-safe pool
        mysql::connection_pool pool(ctx, std::move(params));

        // We can now pass a reference to pool to other threads,
        // and call async_get_connection concurrently without problem.
        // Individual connections are still not thread-safe.
        //]
    }
}

}  // namespace
