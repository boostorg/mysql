//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/connection_pool.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/pool_params.hpp>
#include <boost/mysql/throw_on_error.hpp>

#include <boost/asio/detached.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_suite.hpp>

#include "test_integration/common.hpp"
#include "test_integration/get_endpoint.hpp"
#include "test_integration/run_stackful_coro.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;

BOOST_AUTO_TEST_SUITE(test_connection_pool)

BOOST_AUTO_TEST_CASE(get_return_connection)
{
    run_stackful_coro([](boost::asio::yield_context yield) {
        pool_params params{connect_params()
                               .set_tcp_address(get_hostname())
                               .set_username(default_user)
                               .set_password(default_passwd)
                               .set_database(default_db)};
        diagnostics diag;
        error_code ec;

        connection_pool pool(yield.get_executor(), std::move(params));
        pool.async_run([](error_code ec) { throw_on_error(ec); });

        // Get a connection
        auto conn = pool.async_get_connection(diag, yield[ec]);
        throw_on_error(ec, diag);

        // Check that the connection works
        BOOST_TEST(conn.valid());
        conn->async_ping(diag, yield[ec]);
        throw_on_error(ec, diag);

        // The connection is returned when conn is destroyed
    });
}

BOOST_AUTO_TEST_SUITE_END()
