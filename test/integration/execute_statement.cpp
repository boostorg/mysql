//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "integration_test_common.hpp"
#include <forward_list>

using namespace boost::mysql::test;
using boost::mysql::value;
using boost::mysql::error_code;
using boost::mysql::error_info;
using boost::mysql::errc;
using boost::mysql::prepared_statement;
using boost::mysql::socket_connection;
using boost::asio::ip::tcp;

BOOST_AUTO_TEST_SUITE(test_execute_statement)

template <typename Stream>
prepared_statement<Stream> do_prepare(
    network_functions<Stream>* net,
    socket_connection<Stream>& conn,
    boost::string_view stmt
)
{
    auto res = net->prepare_statement(conn, stmt);
    res.validate_no_error();
    return std::move(res.value);
}

// Iterator version
BOOST_MYSQL_NETWORK_TEST(iterator_ok_no_params, network_fixture, network_ssl_gen)
{
    this->connect(sample.ssl);
    std::forward_list<value> params;
    auto stmt = do_prepare(sample.net, this->conn, "SELECT * FROM empty_table");
    auto result = sample.net->execute_statement(stmt, params.begin(), params.end()); // execute
    result.validate_no_error();
    BOOST_TEST(result.value.valid());
}

BOOST_MYSQL_NETWORK_TEST(iterator_ok_with_params, network_fixture, network_ssl_gen)
{
    this->connect(sample.ssl);
    std::forward_list<value> params { value("item"), value(42) };
    auto stmt = do_prepare(sample.net, this->conn,
        "SELECT * FROM empty_table WHERE id IN (?, ?)");
    auto result = sample.net->execute_statement(stmt, params.begin(), params.end());
    result.validate_no_error();
    BOOST_TEST(result.value.valid());
}

BOOST_MYSQL_NETWORK_TEST(iterator_mismatched_num_params, network_fixture, network_ssl_gen)
{
    this->connect(sample.ssl);
    std::forward_list<value> params { value("item") };
    auto stmt = do_prepare(sample.net, this->conn,
        "SELECT * FROM empty_table WHERE id IN (?, ?)");
    auto result = sample.net->execute_statement(stmt, params.begin(), params.end());
    result.validate_error(errc::wrong_num_params,
        {"param", "2", "1", "statement", "execute"});
    BOOST_TEST(!result.value.valid());
}

BOOST_MYSQL_NETWORK_TEST(iterator_server_error, network_fixture, network_ssl_gen)
{
    this->connect(sample.ssl);
    this->start_transaction();
    std::forward_list<value> params { value("f0"), value("bad_date") };
    auto stmt = do_prepare(sample.net, this->conn,
        "INSERT INTO inserts_table (field_varchar, field_date) VALUES (?, ?)");
    auto result = sample.net->execute_statement(stmt, params.begin(), params.end());
    result.validate_error(errc::truncated_wrong_value,
        {"field_date", "bad_date", "incorrect date value"});
    BOOST_TEST(!result.value.valid());
}

// Container version
BOOST_MYSQL_NETWORK_TEST(container_ok_no_params, network_fixture, network_ssl_gen)
{
    this->connect(sample.ssl);
    auto stmt = do_prepare(sample.net, this->conn, "SELECT * FROM empty_table");
    auto result = sample.net->execute_statement(stmt, std::vector<value>()); // execute
    result.validate_no_error();
    BOOST_TEST(result.value.valid());
}

BOOST_MYSQL_NETWORK_TEST(container_ok_with_params, network_fixture, network_ssl_gen)
{
    this->connect(sample.ssl);
    auto stmt = do_prepare(sample.net, this->conn, "SELECT * FROM empty_table WHERE id IN (?, ?)");
    auto result = sample.net->execute_statement(stmt, make_value_vector("item", 42));
    result.validate_no_error();
    BOOST_TEST(result.value.valid());
}

BOOST_MYSQL_NETWORK_TEST(container_mismatched_num_params, network_fixture, network_ssl_gen)
{
    this->connect(sample.ssl);
    auto stmt = do_prepare(sample.net, this->conn,
        "SELECT * FROM empty_table WHERE id IN (?, ?)");
    auto result = sample.net->execute_statement(stmt, make_value_vector("item"));
    result.validate_error(errc::wrong_num_params,
        {"param", "2", "1", "statement", "execute"});
    BOOST_TEST(!result.value.valid());
}

BOOST_MYSQL_NETWORK_TEST(container_server_error, network_fixture, network_ssl_gen)
{
    this->connect(sample.ssl);
    this->start_transaction();
    auto stmt = do_prepare(sample.net, this->conn,
        "INSERT INTO inserts_table (field_varchar, field_date) VALUES (?, ?)");
    auto result = sample.net->execute_statement(stmt, make_value_vector("f0", "bad_date"));
    result.validate_error(errc::truncated_wrong_value,
        {"field_date", "bad_date", "incorrect date value"});
    BOOST_TEST(!result.value.valid());
}

// Other containers
BOOST_FIXTURE_TEST_CASE(no_statement_params_variable, network_fixture<tcp::socket>)
{
    this->connect(boost::mysql::ssl_mode::disable);
    auto stmt = this->conn.prepare_statement(
        "SELECT * FROM empty_table");
    auto result = stmt.execute(boost::mysql::no_statement_params);
    BOOST_TEST(result.valid());
}

BOOST_FIXTURE_TEST_CASE(c_array, network_fixture<tcp::socket>)
{
    this->connect(boost::mysql::ssl_mode::disable);
    value arr [] = { value("hola"), value(10) };
    auto stmt = this->conn.prepare_statement(
        "SELECT * FROM empty_table WHERE id IN (?, ?)");
    auto result = stmt.execute(arr);
    BOOST_TEST(result.valid());
}


BOOST_AUTO_TEST_SUITE_END() // test_execute_statement
