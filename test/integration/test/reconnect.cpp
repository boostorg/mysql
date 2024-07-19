//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/common_server_errc.hpp>
#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/ssl_mode.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/post.hpp>
#include <boost/core/span.hpp>
#include <boost/test/data/monomorphic.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>

#include "test_common/create_basic.hpp"
#include "test_common/network_result.hpp"
#include "test_integration/any_connection_fixture.hpp"
#include "test_integration/common.hpp"
#include "test_integration/server_features.hpp"
#include "test_integration/spotchecks_helpers.hpp"
#include "test_integration/tcp_connection_fixture.hpp"

using namespace boost::mysql::test;
using namespace boost::mysql;
namespace asio = boost::asio;
using boost::test_tools::per_element;

namespace {

BOOST_AUTO_TEST_SUITE(test_reconnect)

auto connection_samples = network_functions_connection::sync_and_async();
auto any_samples = network_functions_any::sync_and_async();
auto any_samples_grid = boost::unit_test::data::make(any_samples) *
                        boost::unit_test::data::make({ssl_mode::disable, ssl_mode::require});

// Old connection can reconnect after close if the stream is not SSL
BOOST_DATA_TEST_CASE_F(tcp_connection_fixture, reconnect_after_close_connection, connection_samples)
{
    const network_functions_connection& fn = sample;

    // Connect and use the connection
    results r;
    fn.connect(conn, get_tcp_endpoint(), connect_params_builder().build_hparams()).validate_no_error();
    fn.execute_query(conn, "SELECT * FROM empty_table", r).validate_no_error();

    // Close
    fn.close(conn).validate_no_error();

    // Reopen and use the connection normally
    fn.connect(conn, get_tcp_endpoint(), connect_params_builder().build_hparams()).validate_no_error();
    fn.execute_query(conn, "SELECT * FROM empty_table", r).validate_no_error();
}

// any_connection can reconnect after close, even if the stream uses ssl
BOOST_DATA_TEST_CASE_F(any_connection_fixture, reconnect_after_close_any, any_samples_grid, p0, p1)
{
    const network_functions_any& fn = p0;
    ssl_mode mode = p1;

    // Connect and use the connection
    results r;
    fn.connect(conn, connect_params_builder().ssl(mode).build()).validate_no_error();
    fn.execute_query(conn, "SELECT * FROM empty_table", r).validate_no_error();

    // Close
    fn.close(conn).validate_no_error();

    // Reopen and use the connection normally
    fn.connect(conn, connect_params_builder().ssl(mode).build()).validate_no_error();
    fn.execute_query(conn, "SELECT * FROM empty_table", r).validate_no_error();
}

// Old connection can reconnect after handshake failure if the stream is not SSL
BOOST_DATA_TEST_CASE_F(tcp_connection_fixture, reconnect_after_handshake_error_connection, connection_samples)
{
    const network_functions_connection& fn = sample;

    // Error during server handshake
    fn.connect(conn, get_tcp_endpoint(), connect_params_builder().database("bad_db").build_hparams())
        .validate_error(
            common_server_errc::er_dbaccess_denied_error,
            "Access denied for user 'integ_user'@'%' to database 'bad_db'"
        );

    // Reopen with correct parameters and use the connection normally
    results r;
    fn.connect(conn, get_tcp_endpoint(), connect_params_builder().build_hparams()).validate_no_error();
    fn.execute_query(conn, "SELECT * FROM empty_table", r).validate_no_error();
}

// any_connection can reconnect after a handshake failure, even if SSL is used
BOOST_DATA_TEST_CASE_F(any_connection_fixture, reconnect_after_handshake_error_any, any_samples_grid, p0, p1)
{
    const network_functions_any& fn = p0;
    ssl_mode mode = p1;

    // Error during server handshake
    fn.connect(conn, connect_params_builder().ssl(mode).database("bad_db").build())
        .validate_error(
            common_server_errc::er_dbaccess_denied_error,
            "Access denied for user 'integ_user'@'%' to database 'bad_db'"
        );

    // Reopen with correct parameters and use the connection normally
    results r;
    fn.connect(conn, connect_params_builder().ssl(mode).build()).validate_no_error();
    fn.execute_query(conn, "SELECT * FROM empty_table", r).validate_no_error();
}

// any_connection can reconnect while it's connected
BOOST_DATA_TEST_CASE_F(any_connection_fixture, reconnect_while_connected, any_samples_grid, p0, p1)
{
    const network_functions_any& fn = p0;
    ssl_mode mode = p1;

    // Connect and use the connection
    results r;
    fn.connect(conn, connect_params_builder().ssl(mode).build()).validate_no_error();
    fn.execute_query(conn, "SELECT * FROM empty_table", r).validate_no_error();

    // We can safely connect again
    fn.connect(conn, connect_params_builder().ssl(mode).credentials("root", "").build()).validate_no_error();

    // We've logged in as root
    fn.execute_query(conn, "SELECT CURRENT_USER()", r).validate_no_error();
    BOOST_TEST(r.rows() == makerows(1, "root@%"), per_element());
}

BOOST_FIXTURE_TEST_CASE(reconnect_after_cancel, any_connection_fixture)
{
    // Setup
    auto connect_prms = default_connect_params();
    results r;
    connect();

    // Kick an operation that ends up cancelled
    asio::cancellation_signal sig;
    auto netres = conn.async_execute("DO SLEEP(2)", r, as_netresult_t{sig.slot()});

    // Return to the event loop and emit the signal
    asio::post(asio::bind_executor(ctx.get_executor(), [&]() {
        // Emit the signal
        sig.emit(asio::cancellation_type::terminal);
    }));

    // Wait for the operation to finish
    std::move(netres).validate_error(asio::error::operation_aborted);

    // We can connect again and use the connection
    connect();
    conn.async_execute("SELECT 42", r, as_netresult).validate_no_error();
}

// any_connection can change the stream type used by successive connect calls.
// We need to split this test in two (TCP and UNIX), so UNIX cases don't run on Windows.
struct change_stream_type_fixture : any_connection_fixture
{
    // A test case sample
    struct test_case_t
    {
        string_view name;
        const network_functions_any& fns;
        connect_params first_params;
        connect_params second_params;
    };

    // Runs the actual test
    void run(boost::span<const test_case_t> test_cases)
    {
        for (const auto& tc : test_cases)
        {
            BOOST_TEST_CONTEXT(tc.name)
            {
                // Connect with the first stream type
                tc.fns.connect(conn, tc.first_params).validate_no_error();
                tc.fns.ping(conn).validate_no_error();

                // Connect with the second stream type
                tc.fns.connect(conn, tc.second_params).validate_no_error();
                tc.fns.ping(conn).validate_no_error();
            }
        }
    }

    connect_params tcp_params{connect_params_builder().ssl(ssl_mode::disable).build()};
    connect_params tcp_ssl_params{connect_params_builder().ssl(ssl_mode::require).build()};
};

// TCP cases. Note that some sync cases are not included, to save testing time
BOOST_FIXTURE_TEST_CASE(change_stream_type_tcp, change_stream_type_fixture)
{
    test_case_t test_cases[] = {
        {"sync_tcp_tcpssl",  any_samples[0], tcp_params,     tcp_ssl_params},
        {"async_tcp_tcpssl", any_samples[1], tcp_params,     tcp_ssl_params},
        {"async_tcpssl_tcp", any_samples[1], tcp_ssl_params, tcp_params    },
    };
    run(test_cases);
}

#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
// UNIX cases. Note that some sync cases are not included, to save testing time
BOOST_TEST_DECORATOR(*run_if(&server_features::unix_sockets))
BOOST_FIXTURE_TEST_CASE(change_stream_type_unix, change_stream_type_fixture)
{
    auto unix_params = connect_params_builder().set_unix().build();
    test_case_t test_cases[] = {
        {"sync_unix_tcpssl",  any_samples[0], unix_params,    tcp_ssl_params},
        {"async_unix_tcpssl", any_samples[1], unix_params,    tcp_ssl_params},
        {"async_tcpssl_unix", any_samples[1], tcp_ssl_params, unix_params   },
        {"async_tcp_unix",    any_samples[1], tcp_params,     unix_params   },
    };
    run(test_cases);
}
#endif

BOOST_AUTO_TEST_SUITE_END()  // test_reconnect

}  // namespace
