//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/results.hpp>

#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/error.hpp>
#include <boost/optional/optional.hpp>
#include <boost/optional/optional_io.hpp>
#include <boost/test/unit_test.hpp>

#include <cstdint>

#include "test_common/network_result.hpp"
#include "test_integration/any_connection_fixture.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;
namespace asio = boost::asio;
using boost::test_tools::per_element;

BOOST_AUTO_TEST_SUITE(test_connection_id)

// connection_id
std::uint32_t call_connection_id(any_connection& conn)
{
    results r;
    conn.async_execute("SELECT CONNECTION_ID()", r, as_netresult).validate_no_error();
    return static_cast<std::uint32_t>(r.rows().at(0).at(0).as_uint64());
}

BOOST_FIXTURE_TEST_CASE(success, any_connection_fixture)
{
    // Before connection, connection_id returns an empty optional
    BOOST_TEST(conn.connection_id() == boost::optional<std::uint32_t>());

    // Connect
    connect();

    // The returned id matches CONNECTION_ID()
    auto expected_id = call_connection_id(conn);
    BOOST_TEST(conn.connection_id() == boost::optional<std::uint32_t>(expected_id));

    // Calling reset connection doesn't change the ID
    conn.async_reset_connection(as_netresult).validate_no_error();
    expected_id = call_connection_id(conn);
    BOOST_TEST(conn.connection_id() == boost::optional<std::uint32_t>(expected_id));

    // Close the connection
    conn.async_close(as_netresult).validate_no_error();

    // After session termination, connection_id returns an empty optional
    BOOST_TEST(conn.connection_id() == boost::optional<std::uint32_t>());

    // If we re-establish the session, we get another connection id
    connect();
    auto expected_id_2 = call_connection_id(conn);
    BOOST_TEST(expected_id_2 != expected_id);
    BOOST_TEST(conn.connection_id() == boost::optional<std::uint32_t>(expected_id_2));
}

// After a fatal error (where we didn't call async_close), re-establishing the session
// updates the connection id
BOOST_FIXTURE_TEST_CASE(after_error, any_connection_fixture)
{
    // Connect
    connect();
    auto id1 = call_connection_id(conn);

    // Force a fatal error
    results r;
    asio::cancellation_signal sig;
    auto execute_result = conn.async_execute(
        "DO SLEEP(60)",
        r,
        asio::bind_cancellation_slot(sig.slot(), as_netresult)
    );
    sig.emit(asio::cancellation_type_t::terminal);
    std::move(execute_result).validate_error(asio::error::operation_aborted);

    // The id can be obtained even after the fatal error
    BOOST_TEST(conn.connection_id() == boost::optional<std::uint32_t>(id1));

    // Reconnect
    connect();
    auto id2 = call_connection_id(conn);

    // The new id can be obtained
    BOOST_TEST(conn.connection_id() == boost::optional<std::uint32_t>(id2));
}

// It's safe to obtain the connection id while an operation is in progress
BOOST_FIXTURE_TEST_CASE(op_in_progress, any_connection_fixture)
{
    // Setup
    connect();
    const auto expected_id = call_connection_id(conn);

    // Issue a query
    results r;
    auto execute_result = conn.async_execute("SELECT * FROM three_rows_table", r, as_netresult);

    // While in progress, obtain the connection id. We would usually do this to
    // open a new connection and run a KILL statement. We don't do it here because
    // it's unreliable as a test due to race conditions between sessions in the server.
    BOOST_TEST(conn.connection_id() == boost::optional<std::uint32_t>(expected_id));

    // Finish
    std::move(execute_result).validate_no_error();
}

// It's safe to obtain the connection id while a multi-function operation is in progress
BOOST_FIXTURE_TEST_CASE(multi_function, any_connection_fixture)
{
    // Setup
    connect();
    const auto expected_id = call_connection_id(conn);

    // Start a multi-function operation
    execution_state st;
    conn.async_start_execution("SELECT * FROM three_rows_table", st, as_netresult).validate_no_error();

    // Obtain the connection id
    BOOST_TEST(conn.connection_id() == boost::optional<std::uint32_t>(expected_id));
}

BOOST_AUTO_TEST_SUITE_END()