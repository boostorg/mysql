//
// Copyright (c) 2026 Vladislav Soulgard (vsoulgard at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/common_server_errc.hpp>

#include <boost/mysql/detail/algo_params.hpp>

#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>
#include <boost/mysql/impl/internal/sansio/connect.hpp>

#include <boost/asio/error.hpp>
#include <boost/test/unit_test.hpp>

#include "handshake/handshake_common.hpp"
#include "test_common/create_diagnostics.hpp"
#include "test_unit/algo_test.hpp"
#include "test_unit/create_err.hpp"
#include "test_unit/create_ok.hpp"
#include "test_unit/create_ok_frame.hpp"
#include "test_unit/printing.hpp"

namespace asio = boost::asio;
using namespace boost::mysql::test;
using namespace boost::mysql;
using detail::connection_status;

namespace {

BOOST_AUTO_TEST_SUITE(test_connect)

const void* test_server_address = reinterpret_cast<const void*>(0x1234);

struct fixture : algo_fixture_base
{
    detail::connect_algo algo{
        {test_server_address, handshake_params("example_user", password), false, {}}
    };
};

// Happy path: physical connect, then a successful plaintext handshake.
BOOST_AUTO_TEST_CASE(plaintext_success)
{
    // Setup
    fixture fix;

    // Run the algo
    algo_test()
        .expect_connect()
        .expect_read(server_hello_builder().auth_data(mnp_scramble).build())
        .expect_write(login_request_builder().auth_response(mnp_hash).build())
        .expect_read(create_ok_frame(2, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_connection_id(42)
        .will_set_current_charset(utf8mb4_charset)
        .check(fix);
}

// A network error on the physical connect is returned directly:
// the handshake doesn't run, and no close is requested
// (there's no connection to close). State is untouched: the algo returns
// before touching anything, so every field keeps the fixture's initial value.
BOOST_AUTO_TEST_CASE(plaintext_error_connect_network)
{
    // Setup
    fixture fix;

    // Run the algo
    algo_test()
        .expect_connect(asio::error::connection_refused)
        .check(fix, asio::error::connection_refused);
}

// A handshake error must (a) request a close on the stream and
// (b) return the handshake's error code (the close result is ignored).
BOOST_AUTO_TEST_CASE(plaintext_error_handshake_closes)
{
    // Setup
    fixture fix;

    // Run the algo. The server rejects authentication.
    algo_test()
        .expect_connect()
        .expect_read(server_hello_builder().auth_data(mnp_scramble).build())
        .expect_write(login_request_builder().auth_response(mnp_hash).build())
        .expect_read(err_builder()
                         .seqnum(2)
                         .code(common_server_errc::er_access_denied_error)
                         .message("Denied")
                         .build_frame())
        .expect_close()
        .will_set_status(connection_status::not_connected)
        .check(fix, common_server_errc::er_access_denied_error, create_server_diag("Denied"));
}

// A close request that itself fails must not mask the handshake error:
// connect_algo returns stored_ec_, not the close error.
BOOST_AUTO_TEST_CASE(plaintext_error_handshake_close_also_fails)
{
    // Setup
    fixture fix;

    // Run the algo. The close fails after the handshake already failed.
    algo_test()
        .expect_connect()
        .expect_read(server_hello_builder().auth_data(mnp_scramble).build())
        .expect_write(login_request_builder().auth_response(mnp_hash).build())
        .expect_read(err_builder()
                         .seqnum(2)
                         .code(common_server_errc::er_access_denied_error)
                         .message("Denied")
                         .build_frame())
        .expect_close(asio::error::bad_descriptor)
        .will_set_status(connection_status::not_connected)
        .check(fix, common_server_errc::er_access_denied_error, create_server_diag("Denied"));
}

// Happy path with TLS: connect, server hello advertising SSL,
// SSL request, TLS handshake, login request, OK.
BOOST_AUTO_TEST_CASE(ssl_success)
{
    // Setup: the transport must support SSL for negotiation to include it
    fixture fix;
    fix.st.tls_supported = true;

    // Run the algo
    algo_test()
        .expect_connect()
        .expect_read(server_hello_builder().caps(tls_caps).auth_data(mnp_scramble).build())
        .expect_write(create_ssl_request())
        .expect_ssl_handshake()
        .expect_write(login_request_builder().caps(tls_caps).seqnum(2).auth_response(mnp_hash).build())
        .expect_read(create_ok_frame(3, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_tls_active(true)
        .will_set_capabilities(tls_caps)
        .will_set_connection_id(42)
        .will_set_current_charset(utf8mb4_charset)
        .check(fix);
}

// TLS handshake failing mid-way also triggers a close and returns
// the TLS error (handshake_algo propagates it as its final ec).
BOOST_AUTO_TEST_CASE(ssl_error_handshake_closes)
{
    // Setup
    fixture fix;
    fix.st.tls_supported = true;

    // Run the algo: SSL handshake fails
    algo_test()
        .expect_connect()
        .expect_read(server_hello_builder().caps(tls_caps).auth_data(mnp_scramble).build())
        .expect_write(create_ssl_request())
        .expect_ssl_handshake(asio::error::connection_reset)
        .expect_close()
        .will_set_status(connection_status::not_connected)
        .check(fix, asio::error::connection_reset);
}

// If the handshake fails AFTER the TLS handshake was performed, we must
// shut down the SSL stream before closing the socket.
BOOST_AUTO_TEST_CASE(ssl_error_auth_fails_performs_shutdown)
{
    // Setup
    fixture fix;
    fix.st.tls_supported = true;

    // Run the algo. TLS succeeds, but authentication is rejected.
    algo_test()
        .expect_connect()
        .expect_read(server_hello_builder().caps(tls_caps).auth_data(mnp_scramble).build())
        .expect_write(create_ssl_request())
        .expect_ssl_handshake()
        .expect_write(login_request_builder().caps(tls_caps).seqnum(2).auth_response(mnp_hash).build())
        .expect_read(err_builder()
                         .seqnum(3)
                         .code(common_server_errc::er_access_denied_error)
                         .message("Denied")
                         .build_frame())
        .expect_ssl_shutdown()
        .expect_close()
        .will_set_status(connection_status::not_connected)
        .will_set_tls_active(false)
        .check(fix, common_server_errc::er_access_denied_error, create_server_diag("Denied"));
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace
