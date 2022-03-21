//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/connection.hpp>
#include "integration_test_common.hpp"
#include "get_endpoint.hpp"

using namespace boost::mysql::test;
using boost::mysql::connection;
using boost::asio::ip::tcp;

BOOST_AUTO_TEST_SUITE(test_connection)

BOOST_FIXTURE_TEST_CASE(move_constructor_connected_connection, network_fixture)
{
    // First connection
    connection<tcp::socket> first (ctx);
    BOOST_TEST(first.valid());

    // Connect
    first.connect(get_endpoint<tcp::socket>(er_endpoint::valid), params);

    // Construct second connection
    connection<tcp::socket> second (std::move(first));
    BOOST_TEST(!first.valid());
    BOOST_TEST(second.valid());
    second.query("SELECT 1"); // doesn't throw
}

BOOST_FIXTURE_TEST_CASE(move_assignment_from_connected_connection, network_fixture)
{
    // First connection
    connection<tcp::socket> first (ctx);
    connection<tcp::socket> second (ctx);
    BOOST_TEST(first.valid());
    BOOST_TEST(second.valid());

    // Connect
    first.connect(get_endpoint<tcp::socket>(er_endpoint::valid), params);

    // Move
    second = std::move(first);
    BOOST_TEST(!first.valid());
    BOOST_TEST(second.valid());
    second.query("SELECT 1"); // doesn't throw
}

BOOST_AUTO_TEST_SUITE_END() // test_connection
