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
#include <boost/mysql/results.hpp>
#include <boost/mysql/throw_on_error.hpp>

#include <boost/asio/detached.hpp>
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
    return pool_params{connect_params()
                           .set_tcp_address(get_hostname())
                           .set_username(default_user)
                           .set_password(default_passwd)
                           .set_database(default_db)};
}

BOOST_AUTO_TEST_CASE(get_return_connection)
{
    run_stackful_coro([](boost::asio::yield_context yield) {
        diagnostics diag;
        error_code ec;

        connection_pool pool(yield.get_executor(), default_pool_params());
        pool.async_run([](error_code ec) { throw_on_error(ec); });

        // Get a connection
        auto conn = pool.async_get_connection(diag, yield[ec]);
        throw_on_error(ec, diag);

        // Check that the connection works
        BOOST_TEST_REQUIRE(conn.valid());
        conn->async_ping(diag, yield[ec]);
        throw_on_error(ec, diag);

        // The connection is returned when conn is destroyed
    });
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

BOOST_AUTO_TEST_SUITE_END()
