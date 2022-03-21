//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "integration_test_common.hpp"

using namespace boost::mysql::test;
using boost::mysql::error_code;

BOOST_AUTO_TEST_SUITE(test_quit_connection)

BOOST_MYSQL_NETWORK_TEST(active_connection, network_fixture)
{
    setup_and_connect(sample.net);

    // Quit
    conn->quit().validate_no_error();

    // We are no longer able to query
    conn->query("SELECT 1").validate_any_error();
}

BOOST_AUTO_TEST_SUITE_END() // test_quit_connection
