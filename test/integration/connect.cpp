//
// Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "integration_test_common.hpp"

using namespace boost::mysql::test;
using boost::mysql::socket_connection;
using boost::mysql::errc;
using boost::mysql::error_code;

BOOST_AUTO_TEST_SUITE(test_connect)

BOOST_MYSQL_NETWORK_TEST(physical_and_handshake_ok, network_fixture, network_ssl_gen)
{
    auto ep = get_endpoint<Stream>(endpoint_kind::localhost);
    auto result = sample.net->connect(this->conn, ep, this->params);
    result.validate_no_error();

    // We are able to query
    auto query_result = sample.net->query(this->conn, "SELECT 1");
    query_result.validate_no_error();
    query_result.value.read_all(); // discard any result
}

BOOST_MYSQL_NETWORK_TEST(physical_error, network_fixture, network_ssl_gen)
{
    auto ep = get_endpoint<Stream>(endpoint_kind::inexistent);
    auto result = sample.net->connect(this->conn, ep, this->params);
    // depending on system and stream type, error code will be different
    result.validate_any_error({"physical connect failed"});
    BOOST_TEST(!this->conn.next_layer().is_open());
}

BOOST_MYSQL_NETWORK_TEST(physical_ok_handshake_error, network_fixture, network_ssl_gen)
{
    auto ep = get_endpoint<Stream>(endpoint_kind::localhost);
    this->set_credentials("integ_user", "bad_password");
    auto result = sample.net->connect(this->conn, ep, this->params);
    result.validate_error(errc::access_denied_error, {"access denied", "integ_user"});
    BOOST_TEST(!this->conn.next_layer().is_open());
}

BOOST_AUTO_TEST_SUITE_END() // test_connect

