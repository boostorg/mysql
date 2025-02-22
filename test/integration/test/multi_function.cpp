//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/results.hpp>

#include <boost/test/unit_test.hpp>

#include "test_common/network_result.hpp"
#include "test_integration/any_connection_fixture.hpp"
#include "test_integration/connect_params_builder.hpp"

using namespace boost::mysql::test;
using namespace boost::mysql;
using boost::test_tools::per_element;

// Most of the multi-function API is covered in other sections
// (e.g. multi-function with the static interface is covered in static_interface.cpp).
// This file contains the tests than are specific to multi-function and don't belong to any other section.

namespace {

BOOST_AUTO_TEST_SUITE(test_multi_function)

// If we start a multi-function operation and we try to run any other operation, the latter fails without
// undefined behavior
BOOST_FIXTURE_TEST_CASE(status_checks, any_connection_fixture)
{
    connect();

    // Start the operation
    execution_state st;
    conn.async_start_execution("SELECT * FROM empty_table", st, as_netresult).validate_no_error();
    BOOST_TEST_REQUIRE(st.should_read_rows());

    // If we try to start any other operation here, an error is issued
    results r;
    conn.async_execute("SELECT 1", r, as_netresult).validate_error(client_errc::engaged_in_multi_function);
    conn.async_prepare_statement("SELECT 1", as_netresult)
        .validate_error(client_errc::engaged_in_multi_function);
    conn.async_ping(as_netresult).validate_error(client_errc::engaged_in_multi_function);

    // The error is non-fatal: once we finish with the multi-function operation,
    // we can keep using the connection
    auto rv = conn.async_read_some_rows(st, as_netresult).get();
    BOOST_TEST(rv == rows_view(), per_element());
    conn.async_execute("SELECT 1", r, as_netresult).validate_no_error();
}

// We don't mess up with status in case of errors
BOOST_FIXTURE_TEST_CASE(status_checks_errors, any_connection_fixture)
{
    connect();

    // Start the operation, which finishes with an error
    execution_state st;
    conn.async_start_execution("SELECT * FROM bad_table", st, as_netresult)
        .validate_error(
            common_server_errc::er_no_such_table,
            "Table 'boost_mysql_integtests.bad_table' doesn't exist"
        );

    // We can keep using the connection
    results r;
    conn.async_execute("SELECT 1", r, as_netresult).validate_no_error();
}

// connect works to reconnect the connection,
// even if we're in the middle of a multi-function operation
BOOST_FIXTURE_TEST_CASE(connect_during_multi_function, any_connection_fixture)
{
    connect();

    // Start the operation, which finishes with an error
    execution_state st;
    conn.async_start_execution("SELECT * FROM empty_table", st, as_netresult).validate_no_error();
    BOOST_TEST_REQUIRE(st.should_read_rows());

    // We can keep use connect here, and we get a usable connection
    conn.async_connect(connect_params_builder().disable_ssl().build(), as_netresult).validate_no_error();
    conn.async_ping(as_netresult).validate_no_error();
}

// close can be called even if we're in the middle of a multi-function operation
BOOST_FIXTURE_TEST_CASE(close_during_multi_function, any_connection_fixture)
{
    // Connect with TLS enabled
    conn.async_connect(connect_params_builder().build(), as_netresult).validate_no_error();

    // Start the operation, which finishes with an error
    execution_state st;
    conn.async_start_execution("SELECT * FROM empty_table", st, as_netresult).validate_no_error();
    BOOST_TEST_REQUIRE(st.should_read_rows());

    // We can use close here without errors
    conn.async_close(as_netresult).validate_no_error();
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace
