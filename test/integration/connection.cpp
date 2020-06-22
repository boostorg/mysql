//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "boost/mysql/connection.hpp"
#include "integration_test_common.hpp"
#include "get_endpoint.hpp"

using namespace boost::mysql::test;
using boost::mysql::connection;
using boost::mysql::socket_connection;
using boost::asio::ip::tcp;

BOOST_AUTO_TEST_SUITE(test_connection)

BOOST_FIXTURE_TEST_CASE(move_constructor_connected_connection, network_fixture<tcp::socket>)
{
    // First connection
    connection<tcp::socket> first (ctx);
    BOOST_TEST(first.valid());

    // Connect
    first.next_layer().connect(get_endpoint<tcp::socket>(endpoint_kind::localhost));
    first.handshake(params);

    // Construct second connection
    connection<tcp::socket> second (std::move(first));
    BOOST_TEST(!first.valid());
    BOOST_TEST(second.valid());
    second.query("SELECT 1"); // doesn't throw
}

BOOST_FIXTURE_TEST_CASE(move_assignment_from_connected_connection, network_fixture<tcp::socket>)
{
    // First connection
    connection<tcp::socket> first (ctx);
    connection<tcp::socket> second (ctx);
    BOOST_TEST(first.valid());
    BOOST_TEST(second.valid());

    // Connect
    first.next_layer().connect(get_endpoint<tcp::socket>(endpoint_kind::localhost));
    first.handshake(params);

    // Move
    second = std::move(first);
    BOOST_TEST(!first.valid());
    BOOST_TEST(second.valid());
    second.query("SELECT 1"); // doesn't throw
}

BOOST_AUTO_TEST_SUITE_END() // test_connection

BOOST_AUTO_TEST_SUITE(test_socket_connection)

BOOST_FIXTURE_TEST_CASE(move_constructor_connected_connection, network_fixture<tcp::socket>)
{
    // First connection
    socket_connection<tcp::socket> first (ctx);
    BOOST_TEST(first.valid());

    // Connect
    first.connect(get_endpoint<tcp::socket>(endpoint_kind::localhost), params);

    // Construct second connection
    socket_connection<tcp::socket> second (std::move(first));
    BOOST_TEST(!first.valid());
    BOOST_TEST(second.valid());
    second.query("SELECT 1"); // doesn't throw
}

BOOST_FIXTURE_TEST_CASE(move_assignment_from_connected_connection, network_fixture<tcp::socket>)
{
    // First connection
    socket_connection<tcp::socket> first (ctx);
    socket_connection<tcp::socket> second (ctx);
    BOOST_TEST(first.valid());
    BOOST_TEST(second.valid());

    // Connect
    first.connect(get_endpoint<tcp::socket>(endpoint_kind::localhost), params);

    // Move
    second = std::move(first);
    BOOST_TEST(!first.valid());
    BOOST_TEST(second.valid());
    second.query("SELECT 1"); // doesn't throw
}

BOOST_AUTO_TEST_SUITE_END() // test_socket_connection
