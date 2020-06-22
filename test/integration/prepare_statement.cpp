//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "integration_test_common.hpp"

using namespace boost::mysql::test;
using boost::mysql::error_code;
using boost::mysql::error_info;
using boost::mysql::errc;
using boost::mysql::prepared_statement;
using boost::mysql::connection;

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

BOOST_AUTO_TEST_SUITE_END() // test_prepare_statement
