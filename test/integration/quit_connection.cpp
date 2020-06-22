//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "integration_test_common.hpp"

using namespace boost::mysql::test;
using boost::mysql::error_code;

BOOST_AUTO_TEST_SUITE(test_quit_connection)

BOOST_MYSQL_NETWORK_TEST(active_connection, network_fixture, network_ssl_gen)
{
    this->connect(sample.ssl);

    // Quit
    auto result = sample.net->quit(this->conn);
    result.validate_no_error();

    // We are no longer able to query
    auto query_result = sample.net->query(this->conn, "SELECT 1");
    query_result.validate_any_error();
}

BOOST_AUTO_TEST_SUITE_END() // test_quit_connection
