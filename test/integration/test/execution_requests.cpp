//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/results.hpp>

#include <boost/test/unit_test.hpp>

#include <forward_list>

#include "test_common/create_basic.hpp"
#include "test_common/network_result.hpp"
#include "test_integration/any_connection_fixture.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;
using boost::test_tools::per_element;

// Cover all possible execution requests for execute() and async_execute()

namespace {

BOOST_AUTO_TEST_SUITE(test_execution_requests)

BOOST_FIXTURE_TEST_CASE(query, any_connection_fixture)
{
    // Setup
    connect();
    results r;
    execution_state st;

    // execute
    conn.async_execute("SELECT 1", r, as_netresult).validate_no_error();
    BOOST_TEST(r.rows() == makerows(1, 1), per_element());

    // start execution
    conn.async_start_execution("SELECT 1", st, as_netresult).validate_no_error();
    auto rws = conn.async_read_some_rows(st, as_netresult).get();
    BOOST_TEST(rws == makerows(1, 1), per_element());
}

BOOST_FIXTURE_TEST_CASE(stmt_tuple, any_connection_fixture)
{
    // Setup
    connect();
    results r;
    execution_state st;
    auto stmt = conn.async_prepare_statement("SELECT ?", as_netresult).get();
    BOOST_TEST_REQUIRE(stmt.num_params() == 1u);

    // execute
    conn.async_execute(stmt.bind("42"), r, as_netresult).validate_no_error();
    BOOST_TEST(r.rows() == makerows(1, "42"), per_element());

    // start execution
    conn.async_start_execution(stmt.bind(90), st, as_netresult).validate_no_error();
    auto rws = conn.async_read_some_rows(st, as_netresult).get();
    BOOST_TEST(rws == makerows(1, 90), per_element());
}

BOOST_FIXTURE_TEST_CASE(stmt_tuple_error, any_connection_fixture)
{
    // Setup
    connect();
    results r;
    execution_state st;
    auto stmt = conn.async_prepare_statement("SELECT ?", as_netresult).get();
    BOOST_TEST_REQUIRE(stmt.num_params() == 1u);

    // execute
    conn.async_execute(stmt.bind("42", 200), r, as_netresult).validate_error(client_errc::wrong_num_params);

    // start execution
    conn.async_start_execution(stmt.bind(), st, as_netresult).validate_error(client_errc::wrong_num_params);
}

BOOST_FIXTURE_TEST_CASE(stmt_range, any_connection_fixture)
{
    // Setup
    connect();
    results r;
    execution_state st;
    std::forward_list<field_view> params{field_view(42), field_view("abc")};
    auto stmt = conn.async_prepare_statement("SELECT ?, ?", as_netresult).get();
    BOOST_TEST_REQUIRE(stmt.num_params() == 2u);

    // execute
    conn.async_execute(stmt.bind(params.begin(), params.end()), r, as_netresult).validate_no_error();
    BOOST_TEST(r.rows() == makerows(2, 42, "abc"), per_element());

    // start execution
    conn.async_start_execution(stmt.bind(params.begin(), params.end()), st, as_netresult).validate_no_error();
    auto rws = conn.async_read_some_rows(st, as_netresult).get();
    BOOST_TEST(rws == makerows(2, 42, "abc"), per_element());
}

BOOST_FIXTURE_TEST_CASE(stmt_range_error, any_connection_fixture)
{
    // Setup
    connect();
    results r;
    execution_state st;
    std::forward_list<field_view> params{field_view(42)};
    auto stmt = conn.async_prepare_statement("SELECT ?, ?", as_netresult).get();
    BOOST_TEST_REQUIRE(stmt.num_params() == 2u);

    // execute
    conn.async_execute(stmt.bind(params.begin(), params.end()), r, as_netresult)
        .validate_error(client_errc::wrong_num_params);

    // start execution
    conn.async_start_execution(stmt.bind(params.begin(), params.end()), st, as_netresult)
        .validate_error(client_errc::wrong_num_params);
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace