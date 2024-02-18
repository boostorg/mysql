//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/any_address.hpp>
#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/common_server_errc.hpp>
#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/connection.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/ssl_mode.hpp>
#include <boost/mysql/throw_on_error.hpp>

#include <boost/asio/deferred.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/experimental/cancellation_condition.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/core/span.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_suite.hpp>

#include <exception>

#include "test_common/netfun_maker.hpp"
#include "test_integration/common.hpp"
#include "test_integration/get_endpoint.hpp"
#include "test_integration/network_test.hpp"
#include "test_integration/run_stackful_coro.hpp"

using namespace boost::mysql::test;
using namespace boost::mysql;
using boost::asio::deferred;
using boost::asio::experimental::make_parallel_group;
using boost::asio::experimental::wait_for_one;

namespace {

// clang-format off
auto samples_with_reconnection = create_network_samples({
    "tcp_sync_errc",
    "tcp_async_callback",
    "any_tcp_sync_errc",
    "any_tcp_async_callback",
#if BOOST_ASIO_HAS_LOCAL_SOCKETS
    "any_unix_sync_errc",
#endif
});

auto samples_any = create_network_samples({
    "any_tcp_sync_errc",
    "any_tcp_async_callback",
#if BOOST_ASIO_HAS_LOCAL_SOCKETS
    "any_unix_sync_errc",
#endif
});
// clang-format on

BOOST_AUTO_TEST_SUITE(test_reconnect)

struct reconnect_fixture : network_fixture
{
    void do_query_ok()
    {
        results result;
        conn->execute("SELECT * FROM empty_table", result).get();
        BOOST_TEST(result.rows().empty());
    }
};

BOOST_MYSQL_NETWORK_TEST(reconnect_after_close, reconnect_fixture, samples_with_reconnection)
{
    setup(sample.net);

    // Connect and use the connection
    connect();
    do_query_ok();

    // Close
    conn->close().validate_no_error();

    // Reopen and use the connection normally
    connect();
    do_query_ok();
}

BOOST_MYSQL_NETWORK_TEST(reconnect_after_handshake_error, reconnect_fixture, samples_with_reconnection)
{
    setup(sample.net);

    // Error during server handshake
    params.set_database("bad_database");
    conn->connect(params).validate_error(
        common_server_errc::er_dbaccess_denied_error,
        {"database", "bad_database"}
    );

    // Reopen with correct parameters and use the connection normally
    params.set_database("boost_mysql_integtests");
    connect();
    do_query_ok();
}

BOOST_MYSQL_NETWORK_TEST(reconnect_while_connected, reconnect_fixture, samples_any)
{
    setup(sample.net);

    // Connect and use the connection
    connect();
    do_query_ok();

    // We can safely connect again
    params.set_username("root");
    params.set_password("");
    connect();

    // We've logged in as root
    results r;
    conn->execute("SELECT CURRENT_USER()", r).validate_no_error();
    BOOST_TEST(r.rows().at(0).at(0).as_string().starts_with("root"));
}

// parallel_group doesn't work with this macro. See https://github.com/chriskohlhoff/asio/issues/1398
#ifndef BOOST_ASIO_USE_TS_EXECUTOR_AS_DEFAULT
BOOST_AUTO_TEST_CASE(reconnect_after_cancel)
{
    run_stackful_coro([](boost::asio::yield_context yield) {
        // Setup
        auto connect_prms = default_connect_params();
        any_connection conn(yield.get_executor());
        results r;
        boost::mysql::error_code ec;
        boost::mysql::diagnostics diag;

        // Connect
        conn.async_connect(connect_prms, diag, yield[ec]);
        boost::mysql::throw_on_error(ec, diag);

        // Kick an operation that ends up cancelled
        auto wait_result = make_parallel_group(
                               conn.async_execute("DO SLEEP(2)", r, deferred),
                               boost::asio::post(yield.get_executor(), deferred)
        )
                               .async_wait(wait_for_one(), yield);

        // Verify this was the case
        BOOST_TEST(std::get<0>(wait_result)[1] == 0u);  // post completed first
        BOOST_TEST(std::get<1>(wait_result) == boost::asio::error::operation_aborted);

        // We can connect again
        conn.async_connect(connect_prms, diag, yield[ec]);
        boost::mysql::throw_on_error(ec, diag);
    });
}
#endif

// any_connection can change the stream type used by successive connect calls.
// We need to split this test in two (TCP and UNIX), so UNIX cases don't run on Windows.
struct change_stream_type_fixture : network_fixture_base
{
    // Functions, to run sync and async with the same code
    using netmaker_connect = netfun_maker_mem<void, any_connection, const connect_params&>;
    using netmaker_ping = netfun_maker_mem<void, any_connection>;

    struct functions_t
    {
        netmaker_connect::signature connect;
        netmaker_ping::signature ping;
    };

    // A test case sample
    struct test_case_t
    {
        const char* name;
        functions_t fns;
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
                any_connection conn{ctx.get_executor()};

                // Connect with the first stream type
                tc.fns.connect(conn, tc.first_params).validate_no_error();
                tc.fns.ping(conn).validate_no_error();

                // Connect with the second stream type
                tc.fns.connect(conn, tc.second_params).validate_no_error();
                tc.fns.ping(conn).validate_no_error();
            }
        }
    }

    functions_t sync_fns{
        netmaker_connect::sync_errc(&any_connection::connect),
        netmaker_ping::sync_errc(&any_connection::ping),
    };

    functions_t async_fns{
        netmaker_connect::async_errinfo(&any_connection::async_connect, false),
        netmaker_ping::async_errinfo(&any_connection::async_ping, false),
    };

    connect_params tcp_params;
    connect_params tcp_ssl_params;

    change_stream_type_fixture()
        : tcp_params(default_connect_params(ssl_mode::disable)),
          tcp_ssl_params(default_connect_params(ssl_mode::require))
    {
    }
};

// TCP cases. Note that some sync cases are not included, to save testing time
BOOST_FIXTURE_TEST_CASE(change_stream_type_tcp, change_stream_type_fixture)
{
    test_case_t test_cases[] = {
        {"sync_tcp_tcpssl",  sync_fns,  tcp_params,     tcp_ssl_params},
        {"async_tcp_tcpssl", async_fns, tcp_params,     tcp_ssl_params},
        {"async_tcpssl_tcp", async_fns, tcp_ssl_params, tcp_params    },
    };
    run(test_cases);
}

// UNIX cases. Note that some sync cases are not included, to save testing time
BOOST_TEST_DECORATOR(*boost::unit_test::label("unix"))
BOOST_FIXTURE_TEST_CASE(change_stream_type_unix, change_stream_type_fixture)
{
    // UNIX connect params
    auto unix_params = default_connect_params();
    unix_params.server_address.emplace_unix_path(default_unix_path);

    test_case_t test_cases[] = {
        {"sync_unix_tcpssl",  sync_fns,  unix_params,    tcp_ssl_params},
        {"async_unix_tcpssl", async_fns, unix_params,    tcp_ssl_params},
        {"async_tcpssl_unix", async_fns, tcp_ssl_params, unix_params   },
        {"async_tcp_unix",    async_fns, tcp_params,     unix_params   },
    };
    run(test_cases);
}

BOOST_AUTO_TEST_SUITE_END()  // test_reconnect

}  // namespace
