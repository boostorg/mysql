//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/prepared_statement.hpp>
#include "integration_test_common.hpp"
#include "tcp_network_fixture.hpp"
#include "streams.hpp"
#include <boost/asio/ip/tcp.hpp>
#include <boost/mysql/tcp.hpp>

using namespace boost::mysql::test;
using boost::mysql::error_code;
using boost::mysql::errc;
using boost::mysql::tcp_prepared_statement;
using boost::mysql::no_statement_params;

BOOST_AUTO_TEST_SUITE(test_prepare_statement)

BOOST_MYSQL_NETWORK_TEST(ok_no_params, network_fixture)
{
    setup_and_connect(sample.net);
    auto stmt = conn->prepare_statement("SELECT * FROM empty_table").get();
    BOOST_TEST_REQUIRE(stmt->valid());
    BOOST_TEST(stmt->id() > 0u);
    BOOST_TEST(stmt->num_params() == 0u);
}

BOOST_MYSQL_NETWORK_TEST(ok_with_params, network_fixture)
{
    setup_and_connect(sample.net);
    auto stmt = conn->prepare_statement("SELECT * FROM empty_table WHERE id IN (?, ?)").get();
    BOOST_TEST_REQUIRE(stmt->valid());
    BOOST_TEST(stmt->id() > 0u);
    BOOST_TEST(stmt->num_params() == 2u);
}

BOOST_MYSQL_NETWORK_TEST(invalid_statement, network_fixture)
{
    setup_and_connect(sample.net);
    auto stmt = conn->prepare_statement("SELECT * FROM bad_table WHERE id IN (?, ?)");
    stmt.validate_error(errc::no_such_table, {"table", "doesn't exist", "bad_table"});
}

// Move operations
BOOST_AUTO_TEST_SUITE(move_operations)

BOOST_FIXTURE_TEST_CASE(move_ctor, tcp_network_fixture)
{
    connect();

    // Get a valid prepared statement and perform a move construction
    tcp_prepared_statement s1 = conn.prepare_statement("SELECT * FROM empty_table");
    tcp_prepared_statement s2 (std::move(s1));

    // Validate valid()
    BOOST_TEST(!s1.valid());
    BOOST_TEST(s2.valid());

    // We can use the 2nd stmt
    auto rows = s2.execute(no_statement_params).read_all();
    BOOST_TEST(rows.empty());
}

BOOST_FIXTURE_TEST_CASE(move_assignment_to_invalid, tcp_network_fixture)
{
    connect();

    // Get a valid resultset and perform a move assignment
    tcp_prepared_statement s1 = conn.prepare_statement("SELECT * FROM empty_table");
    tcp_prepared_statement s2;
    s2 = std::move(s1);

    // Validate valid()
    BOOST_TEST(!s1.valid());
    BOOST_TEST(s2.valid());

    // We can use the 2nd stmt
    auto rows = s2.execute(no_statement_params).read_all();
    BOOST_TEST(rows.empty());
}

BOOST_FIXTURE_TEST_CASE(move_assignment_to_valid, tcp_network_fixture)
{
    connect();
    
    // Get a valid resultset and perform a move assignment
    tcp_prepared_statement s1 = conn.prepare_statement("SELECT * FROM empty_table");
    tcp_prepared_statement s2 = conn.prepare_statement("SELECT * FROM empty_table WHERE id IN (?, ?)");
    s2 = std::move(s1);

    // Validate valid()
    BOOST_TEST(!s1.valid());
    BOOST_TEST(s2.valid());

    // We can use the 2nd resultset
    auto rows = s2.execute(no_statement_params).read_all();
    BOOST_TEST(rows.empty());
}

BOOST_AUTO_TEST_SUITE_END() // move_operations

BOOST_AUTO_TEST_SUITE_END() // test_prepare_statement
