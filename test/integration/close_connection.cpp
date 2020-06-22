//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "integration_test_common.hpp"

using namespace boost::mysql::test;
using boost::mysql::error_code;
using boost::mysql::socket_connection;

BOOST_AUTO_TEST_SUITE(test_close_connection)

BOOST_MYSQL_NETWORK_TEST(active_connection, network_fixture, network_ssl_gen)
{
    // Connect
    this->connect(sample.ssl);

    // Close
    auto result = sample.net->close(this->conn);
    result.validate_no_error();

    // We are no longer able to query
    auto query_result = sample.net->query(this->conn, "SELECT 1");
    query_result.validate_any_error();

    // The stream is closed
    BOOST_TEST(!this->conn.next_layer().is_open());
}

BOOST_MYSQL_NETWORK_TEST(double_close, network_fixture, network_ssl_gen)
{
    // Connect
    this->connect(sample.ssl);

    // Close
    auto result = sample.net->close(this->conn);
    result.validate_no_error();

    // The stream (socket) is closed
    BOOST_TEST(!this->conn.next_layer().is_open());

    // Closing again returns OK (and does nothing)
    result = sample.net->close(this->conn);
    result.validate_no_error();

    // Stream (socket) still closed
    BOOST_TEST(!this->conn.next_layer().is_open());
}

BOOST_MYSQL_NETWORK_TEST(not_open_connection, network_fixture, network_ssl_gen)
{
    auto result = sample.net->close(this->conn);
    result.validate_no_error();
}

BOOST_AUTO_TEST_SUITE_END() // test_close_connection
