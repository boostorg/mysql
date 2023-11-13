//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/connection_pool.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/pool_params.hpp>
#include <boost/mysql/pooled_connection.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/throw_on_error.hpp>

#include <boost/asio/detached.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_suite.hpp>

#include <chrono>

#include "test_common/printing.hpp"
#include "test_integration/common.hpp"
#include "test_integration/get_endpoint.hpp"
#include "test_integration/run_stackful_coro.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;

BOOST_AUTO_TEST_SUITE(test_connection_pool)

pool_params default_pool_params()
{
    pool_params res;
    res.server_address.emplace_host_and_port(get_hostname());
    res.username = default_user;
    res.password = default_passwd;
    res.database = default_db;
    return res;
}

BOOST_AUTO_TEST_CASE(return_connection_with_reset)
{
    run_stackful_coro([](boost::asio::yield_context yield) {
        diagnostics diag;
        error_code ec;
        results r;
        auto params = default_pool_params();
        params.max_size = 1;  // so the same connection gets always returned

        connection_pool pool(yield.get_executor(), std::move(params));
        pool.async_run([](error_code ec) { throw_on_error(ec); });

        // Get a connection
        auto conn = pool.async_get_connection(diag, yield[ec]);
        throw_on_error(ec, diag);

        // Alter session state
        BOOST_TEST_REQUIRE(conn.valid());
        conn->async_execute("SET @myvar = 'abc'", r, diag, yield[ec]);
        throw_on_error(ec, diag);

        // Return the connection
        conn.return_to_pool();

        // Get the same connection again
        conn = pool.async_get_connection(diag, yield[ec]);
        throw_on_error(ec, diag);

        // The same connection is returned, but session state has been cleared
        BOOST_TEST_REQUIRE(conn.valid());
        conn->async_execute("SELECT @myvar", r, diag, yield[ec]);
        BOOST_TEST(r.rows().at(0).at(0) == field_view());
    });
}

BOOST_AUTO_TEST_CASE(return_connection_without_reset)
{
    run_stackful_coro([](boost::asio::yield_context yield) {
        diagnostics diag;
        error_code ec;
        results r;
        auto params = default_pool_params();
        params.max_size = 1;  // so the same connection gets always returned

        connection_pool pool(yield.get_executor(), std::move(params));
        pool.async_run([](error_code ec) { throw_on_error(ec); });

        // Get a connection
        auto conn = pool.async_get_connection(diag, yield[ec]);
        throw_on_error(ec, diag);

        // Alter session state
        BOOST_TEST_REQUIRE(conn.valid());
        conn->async_execute("SET @myvar = 'abc'", r, diag, yield[ec]);
        throw_on_error(ec, diag);

        // Return the connection
        conn.return_to_pool(false);

        // Get the same connection again
        conn = pool.async_get_connection(diag, yield[ec]);
        throw_on_error(ec, diag);

        // The same connection is returned, and no reset has been issued
        BOOST_TEST_REQUIRE(conn.valid());
        conn->async_execute("SELECT @myvar", r, diag, yield[ec]);
        BOOST_TEST(r.rows().at(0).at(0) == field_view("abc"));
    });
}

// pooled_connection destructor is equivalent to return_connection with reset
BOOST_AUTO_TEST_CASE(pooled_connection_destructor)
{
    run_stackful_coro([](boost::asio::yield_context yield) {
        diagnostics diag;
        error_code ec;
        results r;
        auto params = default_pool_params();
        params.max_size = 1;  // so the same connection gets always returned

        connection_pool pool(yield.get_executor(), std::move(params));
        pool.async_run([](error_code ec) { throw_on_error(ec); });

        {
            // Get a connection
            auto conn = pool.async_get_connection(diag, yield[ec]);
            throw_on_error(ec, diag);

            // Alter session state
            BOOST_TEST_REQUIRE(conn.valid());
            conn->async_execute("SET @myvar = 'abc'", r, diag, yield[ec]);
            throw_on_error(ec, diag);
        }

        // Get the same connection again
        auto conn = pool.async_get_connection(diag, yield[ec]);
        throw_on_error(ec, diag);

        // The same connection is returned, but session state has been cleared
        BOOST_TEST_REQUIRE(conn.valid());
        conn->async_execute("SELECT @myvar", r, diag, yield[ec]);
        BOOST_TEST(r.rows().at(0).at(0) == field_view());
    });
}

BOOST_AUTO_TEST_CASE(connections_created_if_required)
{
    run_stackful_coro([](boost::asio::yield_context yield) {
        diagnostics diag;
        error_code ec;
        results r;

        connection_pool pool(yield.get_executor(), default_pool_params());
        pool.async_run([](error_code ec) { throw_on_error(ec); });

        // Get a connection
        auto conn1 = pool.async_get_connection(diag, yield[ec]);
        throw_on_error(ec, diag);

        // Check that it works
        BOOST_TEST_REQUIRE(conn1.valid());
        conn1->async_execute("SET @myvar = '1'", r, diag, yield[ec]);
        throw_on_error(ec, diag);

        // Get another connection. This will create a new one, since the first one is in use
        auto conn2 = pool.async_get_connection(diag, yield[ec]);
        throw_on_error(ec, diag);

        // Check that it works
        BOOST_TEST_REQUIRE(conn1.valid());
        conn2->async_execute("SET @myvar = '2'", r, diag, yield[ec]);
        throw_on_error(ec, diag);

        // They are different connections
        conn1->async_execute("SELECT @myvar", r, diag, yield[ec]);
        throw_on_error(ec, diag);
        BOOST_TEST(r.rows().at(0).at(0) == field_view("1"));
        conn2->async_execute("SELECT @myvar", r, diag, yield[ec]);
        throw_on_error(ec, diag);
        BOOST_TEST(r.rows().at(0).at(0) == field_view("2"));
    });
}

BOOST_AUTO_TEST_CASE(connection_upper_limit)
{
    run_stackful_coro([](boost::asio::yield_context yield) {
        diagnostics diag;
        error_code ec;
        results r;
        auto params = default_pool_params();
        params.max_size = 1;

        connection_pool pool(yield.get_executor(), std::move(params));
        pool.async_run([](error_code ec) { throw_on_error(ec); });

        // Get a connection
        auto conn = pool.async_get_connection(diag, yield[ec]);
        throw_on_error(ec, diag);

        // Getting another connection will block until one is returned.
        // Since we won't return the one we have, the function time outs
        auto conn2 = pool.async_get_connection(std::chrono::milliseconds(1), diag, yield[ec]);
        BOOST_TEST(!conn2.valid());
        BOOST_TEST(ec == client_errc::timeout);
        BOOST_TEST(diag == diagnostics());
    });
}

BOOST_AUTO_TEST_CASE(cancel_run)
{
    run_stackful_coro([](boost::asio::yield_context yield) {
        diagnostics diag;
        error_code ec;
        results r;
        boost::asio::experimental::channel<void(error_code)> run_chan(yield.get_executor(), 1);

        // Construct a pool and run it
        connection_pool pool(yield.get_executor(), default_pool_params());
        pool.async_run([&run_chan](error_code ec) { run_chan.try_send(ec); });

        // Get a connection
        auto conn = pool.async_get_connection(diag, yield[ec]);
        throw_on_error(ec, diag);

        // Cancel. This will make run() return
        pool.cancel();
        run_chan.async_receive(yield[ec]);
        BOOST_TEST(ec == error_code());

        // Cancel again does nothing
        pool.cancel();
    });
}

BOOST_AUTO_TEST_CASE(cancel_get_connection)
{
    run_stackful_coro([](boost::asio::yield_context yield) {
        diagnostics diag;
        error_code ec;
        results r;
        auto params = default_pool_params();
        params.max_size = 1;
        boost::asio::experimental::channel<void(error_code)> run_chan(yield.get_executor(), 1);
        boost::asio::experimental::channel<void(error_code)> getconn_chan(yield.get_executor(), 1);

        // Construct a pool and run it
        connection_pool pool(yield.get_executor(), std::move(params));
        pool.async_run([&run_chan](error_code ec) {
            BOOST_TEST(ec == error_code());
            run_chan.try_send(error_code());
        });

        // Get a connection
        auto conn = pool.async_get_connection(diag, yield[ec]);
        throw_on_error(ec, diag);

        // Try to get a new one. This will not complete, since there is no room for more connections
        pool.async_get_connection(diag, [&](error_code ec, pooled_connection conn) {
            BOOST_TEST(ec == boost::asio::error::operation_aborted);
            BOOST_TEST(!conn.valid());
            getconn_chan.try_send(error_code());
        });

        // Cancel. This will make run and get_connection return
        pool.cancel();
        run_chan.async_receive(yield);
        getconn_chan.async_receive(yield);

        // Calling get_connection after cancel will return operation_aborted
        conn = pool.async_get_connection(diag, yield[ec]);
        BOOST_TEST(!conn.valid());
        BOOST_TEST(ec == boost::asio::error::operation_aborted);
    });
}

// If get_connection failed because connections are failing to
// connect, appropriate diagnostics are returned
BOOST_AUTO_TEST_CASE(get_connection_diagnostics)
{
    run_stackful_coro([](boost::asio::yield_context yield) {
        diagnostics diag;
        error_code ec;
        results r;
        auto params = default_pool_params();
        params.password = "bad";
        boost::asio::experimental::channel<void(error_code)> run_chan(yield.get_executor(), 1);
        boost::asio::experimental::channel<void(error_code)> getconn_chan(yield.get_executor(), 1);

        // Construct a pool and run it. This pool can't ever have
        // valid connections, since credentials are invalid
        connection_pool pool(yield.get_executor(), std::move(params));
        pool.async_run([](error_code ec) { throw_on_error(ec); });

        // Try to get a connection. This times out, but will return
        // the connection's diagnostics, instead
        auto conn = pool.async_get_connection(std::chrono::milliseconds(10), diag, yield[ec]);
        BOOST_TEST(ec == common_server_errc::er_access_denied_error);
        validate_string_contains(diag.server_message(), {"access denied"});
    });
}

// Spotcheck: pool works with unix sockets, too
BOOST_TEST_DECORATOR(*boost::unit_test::label("unix"))
BOOST_AUTO_TEST_CASE(unix_sockets)
{
    run_stackful_coro([](boost::asio::yield_context yield) {
        diagnostics diag;
        error_code ec;
        results r;
        auto params = default_pool_params();
        params.server_address.emplace_unix_path(default_unix_path);

        connection_pool pool(yield.get_executor(), std::move(params));
        pool.async_run([](error_code ec) { throw_on_error(ec); });

        // Get a connection
        auto conn = pool.async_get_connection(diag, yield[ec]);
        throw_on_error(ec, diag);

        // Verify that works
        BOOST_TEST_REQUIRE(conn.valid());
        conn->async_ping(diag, yield[ec]);
        throw_on_error(ec, diag);
    });
}

BOOST_AUTO_TEST_SUITE_END()
