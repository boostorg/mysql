//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/execute_options.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/tcp.hpp>

#include <forward_list>

#include "er_network_variant.hpp"
#include "integration_test_common.hpp"
#include "streams.hpp"
#include "tcp_network_fixture.hpp"
#include "test_common.hpp"

using namespace boost::mysql::test;
using boost::mysql::errc;
using boost::mysql::error_code;
using boost::mysql::execute_options;
using boost::mysql::field_view;

namespace {

auto net_samples = create_network_samples({
    "tcp_sync_errc",
    "tcp_async_callback",
});

BOOST_AUTO_TEST_SUITE(test_execute_statement)

BOOST_AUTO_TEST_SUITE(iterator)

BOOST_MYSQL_NETWORK_TEST(success, network_fixture, all_network_samples())
{
    setup_and_connect(sample.net);

    // Prepare
    conn->prepare_statement("SELECT * FROM empty_table WHERE id IN (?, ?)", *stmt)
        .validate_no_error();

    // Execute
    std::forward_list<field_view> params{field_view("item"), field_view(42)};
    stmt->execute_params(params.begin(), params.end(), execute_options(), *result)
        .validate_no_error();
    BOOST_TEST(result->base().valid());
}

// TODO: this should be a unit test
BOOST_MYSQL_NETWORK_TEST(mismatched_num_params, network_fixture, net_samples)
{
    setup_and_connect(sample.net);

    // Prepare
    conn->prepare_statement("SELECT * FROM empty_table WHERE id IN (?, ?)", *stmt)
        .validate_no_error();

    // Execute
    std::forward_list<field_view> params{field_view("item")};
    stmt->execute_params(params.begin(), params.end(), execute_options(), *result)
        .validate_error(errc::wrong_num_params, {"param", "2", "1", "statement", "execute"});
}

BOOST_MYSQL_NETWORK_TEST(server_error, network_fixture, all_network_samples())
{
    setup_and_connect(sample.net);
    start_transaction();

    // Prepare
    conn->prepare_statement(
            "INSERT INTO inserts_table (field_varchar, field_date) VALUES (?, ?)",
            *stmt
    )
        .validate_no_error();

    // Execute
    std::forward_list<field_view> params{field_view("f0"), field_view("bad_date")};
    stmt->execute_params(params.begin(), params.end(), execute_options(), *result)
        .validate_error(
            errc::truncated_wrong_value,
            {"field_date", "bad_date", "incorrect date value"}
        );
}
BOOST_AUTO_TEST_SUITE_END()

// collection is a wrapper around execute_params, so only
// a subset of tests are run here
BOOST_AUTO_TEST_SUITE(collection)
BOOST_MYSQL_NETWORK_TEST(ok, network_fixture, all_network_samples())
{
    setup_and_connect(sample.net);

    // Prepare
    conn->prepare_statement("SELECT * FROM empty_table WHERE id IN (?, ?)", *stmt)
        .validate_no_error();

    // Execute
    stmt->execute_collection(make_fv_vector("item", 42), *result).validate_no_error();
    BOOST_TEST(result->base().valid());
}

BOOST_MYSQL_NETWORK_TEST(error, network_fixture, all_network_samples())
{
    setup_and_connect(sample.net);
    start_transaction();

    // Prepare
    conn->prepare_statement(
            "INSERT INTO inserts_table (field_varchar, field_date) VALUES (?, ?)",
            *stmt
    )
        .validate_no_error();

    // Execute
    stmt->execute_collection(make_fv_vector("f0", "bad_date"), *result)
        .validate_error(
            errc::truncated_wrong_value,
            {"field_date", "bad_date", "incorrect date value"}
        );
}

// Other containers
BOOST_FIXTURE_TEST_CASE(no_statement_params_variable, tcp_network_fixture)
{
    boost::mysql::tcp_statement stmt;
    boost::mysql::tcp_resultset result;

    connect();
    conn.prepare_statement("SELECT * FROM empty_table", stmt);
    stmt.execute(boost::mysql::no_statement_params, result);
    BOOST_TEST(result.valid());
}

BOOST_FIXTURE_TEST_CASE(array, tcp_network_fixture)
{
    boost::mysql::tcp_statement stmt;
    boost::mysql::tcp_resultset result;

    connect();
    conn.prepare_statement("SELECT * FROM empty_table WHERE id IN (?, ?)", stmt);
    stmt.execute(boost::mysql::make_field_views("hola", 10), result);
    BOOST_TEST(result.valid());
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()  // test_execute_statement

}  // namespace
