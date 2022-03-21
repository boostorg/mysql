//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/execute_params.hpp>
#include "er_network_variant.hpp"
#include "integration_test_common.hpp"
#include "tcp_network_fixture.hpp"
#include "streams.hpp"
#include <forward_list>

using namespace boost::mysql::test;
using boost::mysql::value;
using boost::mysql::error_code;
using boost::mysql::make_execute_params;
using boost::mysql::errc;

BOOST_AUTO_TEST_SUITE(test_execute_statement)

// Iterator version
BOOST_MYSQL_NETWORK_TEST(iterator_ok_no_params, network_fixture)
{
    setup_and_connect(sample.net);

    // Prepare
    auto stmt = conn->prepare_statement("SELECT * FROM empty_table").get();

    // Execute
    std::forward_list<value> params;
    auto result = stmt->execute_params(make_execute_params(params)).get();
    BOOST_TEST(result->valid());
}

BOOST_MYSQL_NETWORK_TEST(iterator_ok_with_params, network_fixture)
{
    setup_and_connect(sample.net);

    // Prepare
    auto stmt = conn->prepare_statement("SELECT * FROM empty_table WHERE id IN (?, ?)").get();

    // Execute
    std::forward_list<value> params { value("item"), value(42) };
    auto result = stmt->execute_params(make_execute_params(params)).get();
    BOOST_TEST(result->valid());
}

BOOST_MYSQL_NETWORK_TEST(iterator_mismatched_num_params, network_fixture)
{
    setup_and_connect(sample.net);

    // Prepare
    auto stmt = conn->prepare_statement("SELECT * FROM empty_table WHERE id IN (?, ?)").get();
    
    // Execute
    std::forward_list<value> params { value("item") };
    auto result = stmt->execute_params(make_execute_params(params));
    result.validate_error(errc::wrong_num_params,
        {"param", "2", "1", "statement", "execute"});
}

BOOST_MYSQL_NETWORK_TEST(iterator_server_error, network_fixture)
{
    setup_and_connect(sample.net);
    start_transaction();

    // Prepare
    auto stmt = conn->prepare_statement("INSERT INTO inserts_table (field_varchar, field_date) VALUES (?, ?)").get();

    // Execute
    std::forward_list<value> params { value("f0"), value("bad_date") };
    auto result = stmt->execute_params(make_execute_params(params));
    result.validate_error(errc::truncated_wrong_value,
        {"field_date", "bad_date", "incorrect date value"});
}

// Container version
BOOST_MYSQL_NETWORK_TEST(container_ok_no_params, network_fixture)
{
    setup_and_connect(sample.net);

    // Prepare
    auto stmt = conn->prepare_statement("SELECT * FROM empty_table").get();

    // Execute
    auto result = stmt->execute_container(std::vector<value>()).get();
    BOOST_TEST(result->valid());
}

BOOST_MYSQL_NETWORK_TEST(container_ok_with_params, network_fixture)
{
    setup_and_connect(sample.net);

    // Prepare
    auto stmt = conn->prepare_statement("SELECT * FROM empty_table WHERE id IN (?, ?)").get();
    auto result = stmt->execute_container(make_value_vector("item", 42)).get();
    BOOST_TEST(result->valid());
}

BOOST_MYSQL_NETWORK_TEST(container_mismatched_num_params, network_fixture)
{
    setup_and_connect(sample.net);

    // Prepare
    auto stmt = conn->prepare_statement("SELECT * FROM empty_table WHERE id IN (?, ?)").get();

    // Execute
    auto result = stmt->execute_container(make_value_vector("item"));
    result.validate_error(errc::wrong_num_params,
        {"param", "2", "1", "statement", "execute"});
}

BOOST_MYSQL_NETWORK_TEST(container_server_error, network_fixture)
{
    setup_and_connect(sample.net);
    start_transaction();

    // Prepare
    auto stmt = conn->prepare_statement("INSERT INTO inserts_table (field_varchar, field_date) VALUES (?, ?)").get();

    // Execute
    auto result = stmt->execute_container(make_value_vector("f0", "bad_date"));
    result.validate_error(errc::truncated_wrong_value,
        {"field_date", "bad_date", "incorrect date value"});
}

// Other containers. We can't use the type-erased functions here
BOOST_FIXTURE_TEST_CASE(no_statement_params_variable, tcp_network_fixture)
{
    connect();
    auto stmt = conn.prepare_statement("SELECT * FROM empty_table");
    auto result = stmt.execute(boost::mysql::no_statement_params);
    BOOST_TEST(result.valid());
}

BOOST_FIXTURE_TEST_CASE(c_array, tcp_network_fixture)
{
    connect();
    auto stmt = conn.prepare_statement("SELECT * FROM empty_table WHERE id IN (?, ?)");
    value arr [] = { value("hola"), value(10) };
    auto result = stmt.execute(arr);
    BOOST_TEST(result.valid());
}


BOOST_AUTO_TEST_SUITE_END() // test_execute_statement
