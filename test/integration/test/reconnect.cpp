//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

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
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_suite.hpp>

#include <exception>

#include "test_integration/common.hpp"
#include "test_integration/get_endpoint.hpp"

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

connect_params base_connect_params()
{
    return connect_params()
        .set_tcp_address(get_hostname())
        .set_username(default_user)
        .set_password(default_passwd)
        .set_database(default_db);
}

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

BOOST_FIXTURE_TEST_CASE(reconnect_after_cancel, network_fixture_base)
{
    boost::asio::spawn(
        ctx.get_executor(),
        [&](boost::asio::yield_context yield) {
            // Setup
            auto connect_prms = base_connect_params();
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
        },
        [](std::exception_ptr err) {
            if (err)
                std::rethrow_exception(err);
        }
    );
    ctx.run();
}

struct change_stream_type_fixture : network_fixture_base
{
    connect_params tcp_ssl_params{base_connect_params()};
    connect_params tcp_params{base_connect_params().set_ssl(ssl_mode::disable)};
    connect_params unix_params{base_connect_params().set_unix_address(default_unix_path)};
    any_connection conn{ctx.get_executor()};
};

BOOST_FIXTURE_TEST_CASE(change_stream_type_tcp_tcpssl, change_stream_type_fixture)
{
    // TCP
    conn.connect(tcp_params);
    conn.ping();

    // TCP with SSL
    conn.connect(tcp_ssl_params);
    conn.ping();
}

BOOST_FIXTURE_TEST_CASE(change_stream_type_tcpssl_tcp, change_stream_type_fixture)
{
    // TCP SSL
    conn.connect(tcp_ssl_params);
    conn.ping();

    // TCP
    conn.connect(tcp_params);
    conn.ping();
}

#if BOOST_ASIO_HAS_LOCAL_SOCKETS

BOOST_FIXTURE_TEST_CASE(change_stream_type_unix_tcpssl, change_stream_type_fixture)
{
    // UNIX
    conn.connect(unix_params);
    conn.ping();

    // TCP SSL
    conn.connect(tcp_ssl_params);
    conn.ping();
}

BOOST_FIXTURE_TEST_CASE(change_stream_type_tcpssl_unix, change_stream_type_fixture)
{
    // TCP SSL
    conn.connect(tcp_ssl_params);
    conn.ping();

    // UNIX
    conn.connect(unix_params);
    conn.ping();
}

#endif

BOOST_AUTO_TEST_SUITE_END()  // test_reconnect

}  // namespace
