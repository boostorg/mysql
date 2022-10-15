//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/connection.hpp>
#include <boost/mysql/tcp.hpp>

#include "get_endpoint.hpp"
#include "integration_test_common.hpp"

using namespace boost::mysql::test;
using boost::asio::ip::tcp;
using boost::mysql::tcp_connection;
using boost::mysql::tcp_resultset;

namespace {

BOOST_AUTO_TEST_SUITE(test_connection)

BOOST_FIXTURE_TEST_CASE(move_constructor_connected_connection, network_fixture)
{
    // First connection
    tcp_connection first(ctx);
    BOOST_TEST(first.valid());

    // Connect and use
    tcp_resultset result;
    first.connect(get_endpoint<tcp::socket>(er_endpoint::valid), params);
    first.query("SELECT 1", result);
    result.read_all();

    // Construct second connection
    tcp_connection second(std::move(first));
    BOOST_TEST_REQUIRE(second.valid());
    second.query("SELECT 1", result);
    BOOST_TEST(result.read_all().at(0).at(0).as_int64() == 1);
}

BOOST_FIXTURE_TEST_CASE(move_assignment_from_connected_connection, network_fixture)
{
    // Connections
    tcp_connection first(ctx);
    tcp_connection second(ctx);

    // Connect and use both
    tcp_resultset result;
    first.connect(get_endpoint<tcp::socket>(er_endpoint::valid), params);
    second.connect(get_endpoint<tcp::socket>(er_endpoint::valid), params);
    first.query("SELECT 1", result);
    result.read_all();
    second.query("SELECT 2", result);
    result.read_all();

    // Move
    second = std::move(first);
    BOOST_TEST(second.valid());
    second.query("SELECT 4", result);
    BOOST_TEST(result.read_all().at(0).at(0).as_int64() == 4);
}

BOOST_AUTO_TEST_SUITE_END()  // test_connection

}  // namespace
