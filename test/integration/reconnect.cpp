//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/ssl/verify_mode.hpp>
#include <boost/mysql/connection.hpp>
#include <boost/test/tools/interface.hpp>
#include <boost/test/unit_test_suite.hpp>
#include "boost/mysql/connection_params.hpp"
#include "boost/mysql/errc.hpp"
#include "integration_test_common.hpp"

using namespace boost::mysql::test;
using boost::mysql::ssl_mode;

BOOST_AUTO_TEST_SUITE(test_reconnect)

struct reconnect_fixture : network_fixture
{
    void do_query_ok()
    {
        auto result = conn->query("SELECT * FROM empty_table").get();
        auto read_result = result->read_all().get();
        BOOST_TEST(read_result.empty());
    }
};

BOOST_MYSQL_NETWORK_TEST_NOSSL(reconnect_after_close, reconnect_fixture)
{
    setup(sample.net);

    // Connect and use the connection
    connect();
    do_query_ok();

    // Close
    conn->close().validate_no_error();

    // Reopen and use the connection normally
    connect();
    do_query_ok();
}

BOOST_MYSQL_NETWORK_TEST_NOSSL(reconnect_after_handshake_error, reconnect_fixture)
{
    setup(sample.net);

    // Error during server handshake
    params.set_database("bad_database");
    conn->connect(er_endpoint::valid, params).validate_error(
        boost::mysql::errc::dbaccess_denied_error, {"database", "bad_database"});

    // Reopen with correct parameters and use the connection normally
    params.set_database("boost_mysql_integtests");
    connect();
    do_query_ok();
}

BOOST_MYSQL_NETWORK_TEST(reconnect_after_physical_connect_error, reconnect_fixture)
{
    setup(sample.net);

    // Error during connection
    conn->connect(er_endpoint::inexistent, params).validate_any_error({"physical connect failed"});

    // Reopen with and use the connection normally
    connect();
    do_query_ok();
}

BOOST_AUTO_TEST_SUITE_END() // test_reconnect

