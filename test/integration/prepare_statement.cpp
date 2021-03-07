//
// Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "boost/mysql/prepared_statement.hpp"
#include "integration_test_common.hpp"
#include <boost/asio/ip/tcp.hpp>

using namespace boost::mysql::test;
using boost::mysql::error_code;
using boost::mysql::error_info;
using boost::mysql::errc;
using boost::mysql::prepared_statement;
using boost::mysql::connection;
using boost::asio::ip::tcp;
using boost::mysql::tcp_prepared_statement;
using boost::mysql::make_values;
using boost::mysql::no_statement_params;

BOOST_AUTO_TEST_SUITE(test_prepare_statement)

BOOST_MYSQL_NETWORK_TEST(ok_no_params, network_fixture, network_ssl_gen)
{
    this->connect(sample.ssl);
    auto stmt = sample.net->prepare_statement(this->conn,
        "SELECT * FROM empty_table");
    stmt.validate_no_error();
    BOOST_TEST_REQUIRE(stmt.value.valid());
    BOOST_TEST(stmt.value.id() > 0u);
    BOOST_TEST(stmt.value.num_params() == 0);
}

BOOST_MYSQL_NETWORK_TEST(ok_with_params, network_fixture, network_ssl_gen)
{
    this->connect(sample.ssl);
    auto stmt = sample.net->prepare_statement(this->conn,
        "SELECT * FROM empty_table WHERE id IN (?, ?)");
    stmt.validate_no_error();
    BOOST_TEST_REQUIRE(stmt.value.valid());
    BOOST_TEST(stmt.value.id() > 0u);
    BOOST_TEST(stmt.value.num_params() == 2);
}

BOOST_MYSQL_NETWORK_TEST(invalid_statement, network_fixture, network_ssl_gen)
{
    this->connect(sample.ssl);
    auto stmt = sample.net->prepare_statement(this->conn,
        "SELECT * FROM bad_table WHERE id IN (?, ?)");
    stmt.validate_error(errc::no_such_table, {"table", "doesn't exist", "bad_table"});
    BOOST_TEST(!stmt.value.valid());
}

// Move operations
BOOST_AUTO_TEST_SUITE(move_operations)

BOOST_FIXTURE_TEST_CASE(move_ctor, network_fixture<tcp::socket>)
{
    // Get a valid prepared statement and perform a move construction
    this->connect(boost::mysql::ssl_mode::disable);
    tcp_prepared_statement s1 = this->conn.prepare_statement("SELECT * FROM empty_table");
    tcp_prepared_statement s2 (std::move(s1));

    // Validate valid()
    BOOST_TEST(!s1.valid());
    BOOST_TEST(s2.valid());

    // We can use the 2nd stmt
    auto rows = s2.execute(no_statement_params).read_all();
    BOOST_TEST(rows.empty());
}

BOOST_FIXTURE_TEST_CASE(move_assignment_to_invalid, network_fixture<tcp::socket>)
{
    // Get a valid resultset and perform a move assignment
    this->connect(boost::mysql::ssl_mode::disable);
    tcp_prepared_statement s1 = this->conn.prepare_statement("SELECT * FROM empty_table");
    tcp_prepared_statement s2;
    s2 = std::move(s1);

    // Validate valid()
    BOOST_TEST(!s1.valid());
    BOOST_TEST(s2.valid());

    // We can use the 2nd stmt
    auto rows = s2.execute(no_statement_params).read_all();
    BOOST_TEST(rows.empty());
}

BOOST_FIXTURE_TEST_CASE(move_assignment_to_valid, network_fixture<tcp::socket>)
{
    // Get a valid resultset and perform a move assignment
    this->connect(boost::mysql::ssl_mode::disable);
    tcp_prepared_statement s1 = this->conn.prepare_statement("SELECT * FROM empty_table");
    tcp_prepared_statement s2 = this->conn.prepare_statement("SELECT * FROM empty_table WHERE id IN (?, ?)");
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
