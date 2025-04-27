//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/client_errc.hpp>

#include <boost/core/span.hpp>

#include <array>
#include <cstring>
#include <vector>

#include "handshake_common.hpp"
#include "test_common/create_diagnostics.hpp"
#include "test_unit/create_err.hpp"
#include "test_unit/create_ok.hpp"
#include "test_unit/create_ok_frame.hpp"

using namespace boost::mysql::test;
using namespace boost::mysql;
using detail::capabilities;
using detail::connection_status;

namespace {

BOOST_AUTO_TEST_SUITE(test_handshake_csha2p)

constexpr std::array<std::uint8_t, 1> fast_auth_ok{{0x03}};
constexpr std::array<std::uint8_t, 1> perform_full_auth{{0x04}};

// Null-terminated password, as required by the plugin
boost::span<const std::uint8_t> null_terminated_password()
{
    return {reinterpret_cast<const std::uint8_t*>(password), std::strlen(password) + 1};
}

// Edge case: we tolerate a direct OK packet in the fast path, without a fast auth OK
BOOST_AUTO_TEST_CASE(ok)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(
            server_hello_builder().auth_plugin("caching_sha2_password").auth_data(csha2p_challenge).build()
        )
        .expect_write(login_request_builder()
                          .auth_plugin("caching_sha2_password")
                          .auth_response(csha2p_response)
                          .build())
        .expect_read(create_ok_frame(2, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .check(fix);
}

// Edge case: we tolerate a direct error packet in the fast path, without a fast auth OK
// (password errors trigger a perform full auth flow)
BOOST_AUTO_TEST_CASE(err)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(
            server_hello_builder().auth_plugin("caching_sha2_password").auth_data(csha2p_challenge).build()
        )
        .expect_write(login_request_builder()
                          .auth_plugin("caching_sha2_password")
                          .auth_response(csha2p_response)
                          .build())
        .expect_read(err_builder()
                         .seqnum(2)
                         .code(common_server_errc::er_access_denied_error)
                         .message("Denied")
                         .build_frame())
        .will_set_capabilities(min_caps)  // incidental
        .will_set_connection_id(42)       // incidental
        .check(fix, common_server_errc::er_access_denied_error, create_server_diag("Denied"));
}

// At the moment, this plugin requires TLS, so this is an error
BOOST_AUTO_TEST_CASE(fullauth)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder()
                         .caps(tls_caps)
                         .auth_plugin("caching_sha2_password")
                         .auth_data(csha2p_challenge)
                         .build())
        .expect_write(login_request_builder()
                          .auth_plugin("caching_sha2_password")
                          .auth_response(csha2p_response)
                          .build())
        .expect_read(create_more_data_frame(2, perform_full_auth))
        .will_set_capabilities(min_caps)
        .will_set_connection_id(42)
        .check(fix, client_errc::auth_plugin_requires_ssl);
}

// Receiving an unknown more data frame (something != fullauth or fastok) is illegal
BOOST_AUTO_TEST_CASE(moredata)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(
            server_hello_builder().auth_plugin("caching_sha2_password").auth_data(csha2p_challenge).build()
        )
        .expect_write(login_request_builder()
                          .auth_plugin("caching_sha2_password")
                          .auth_response(csha2p_response)
                          .build())
        .expect_read(create_more_data_frame(2, std::vector<std::uint8_t>{10, 20, 30}))
        .will_set_capabilities(min_caps)
        .will_set_connection_id(42)
        .check(fix, client_errc::bad_handshake_packet_type);
}

// Usual success path when using the fast track
BOOST_AUTO_TEST_CASE(fastok_ok)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(
            server_hello_builder().auth_plugin("caching_sha2_password").auth_data(csha2p_challenge).build()
        )
        .expect_write(login_request_builder()
                          .auth_plugin("caching_sha2_password")
                          .auth_response(csha2p_response)
                          .build())
        .expect_read(create_more_data_frame(2, fast_auth_ok))
        .expect_read(create_ok_frame(3, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .check(fix);
}

// Password errors don't trigger this path (they always go through full auth),
// but other errors (like incorrect database) trigger this path
// TODO: uncomment integ test
BOOST_AUTO_TEST_CASE(fastok_err)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(
            server_hello_builder().auth_plugin("caching_sha2_password").auth_data(csha2p_challenge).build()
        )
        .expect_write(login_request_builder()
                          .auth_plugin("caching_sha2_password")
                          .auth_response(csha2p_response)
                          .build())
        .expect_read(create_more_data_frame(2, fast_auth_ok))
        .expect_read(err_builder()
                         .seqnum(3)
                         .code(common_server_errc::er_access_denied_error)
                         .message("Denied")
                         .build_frame())
        .will_set_capabilities(min_caps)  // incidental
        .will_set_connection_id(42)       // incidental
        .check(fix, common_server_errc::er_access_denied_error, create_server_diag("Denied"));
}

// Receiving two consecutive more_data frames with fast OK contents is illegal
BOOST_AUTO_TEST_CASE(fastok_fastok)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(
            server_hello_builder().auth_plugin("caching_sha2_password").auth_data(csha2p_challenge).build()
        )
        .expect_write(login_request_builder()
                          .auth_plugin("caching_sha2_password")
                          .auth_response(csha2p_response)
                          .build())
        .expect_read(create_more_data_frame(2, fast_auth_ok))
        .expect_read(create_more_data_frame(3, fast_auth_ok))
        .will_set_capabilities(min_caps)
        .will_set_connection_id(42)
        .check(fix, client_errc::bad_handshake_packet_type);
}

// Receiving a full auth request after a fast track OK is illegal
BOOST_AUTO_TEST_CASE(fastok_fullauth)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(
            server_hello_builder().auth_plugin("caching_sha2_password").auth_data(csha2p_challenge).build()
        )
        .expect_write(login_request_builder()
                          .auth_plugin("caching_sha2_password")
                          .auth_response(csha2p_response)
                          .build())
        .expect_read(create_more_data_frame(2, fast_auth_ok))
        .expect_read(create_more_data_frame(3, perform_full_auth))
        .will_set_capabilities(min_caps)
        .will_set_connection_id(42)
        .check(fix, client_errc::bad_handshake_packet_type);
}

// Receiving an unknown data frame after a fast track OK fails as expected
BOOST_AUTO_TEST_CASE(fastok_moredata)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(
            server_hello_builder().auth_plugin("caching_sha2_password").auth_data(csha2p_challenge).build()
        )
        .expect_write(login_request_builder()
                          .auth_plugin("caching_sha2_password")
                          .auth_response(csha2p_response)
                          .build())
        .expect_read(create_more_data_frame(2, fast_auth_ok))
        .expect_read(create_more_data_frame(3, std::vector<std::uint8_t>{10, 20, 30}))
        .will_set_capabilities(min_caps)
        .will_set_connection_id(42)
        .check(fix, client_errc::bad_handshake_packet_type);
}

// Receiving an auth switch after a fast track OK fails as expected
// TODO: move this to the generic section
BOOST_AUTO_TEST_CASE(fastok_authswitch)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(
            server_hello_builder().auth_plugin("caching_sha2_password").auth_data(csha2p_challenge).build()
        )
        .expect_write(login_request_builder()
                          .auth_plugin("caching_sha2_password")
                          .auth_response(csha2p_response)
                          .build())
        .expect_read(create_more_data_frame(2, fast_auth_ok))
        .expect_read(create_auth_switch_frame(3, "mysql_native_password", mnp_challenge))
        .will_set_capabilities(min_caps)
        .will_set_connection_id(42)
        .check(fix, client_errc::bad_handshake_packet_type);
}

// Auth switch flows with fast OK work
BOOST_AUTO_TEST_CASE(authswitch_fastok_ok)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(
            server_hello_builder().auth_plugin("mysql_native_password").auth_data(mnp_challenge).build()
        )
        .expect_write(
            login_request_builder().auth_plugin("mysql_native_password").auth_response(mnp_response).build()
        )
        .expect_read(create_auth_switch_frame(2, "caching_sha2_password", csha2p_challenge))
        .expect_write(create_frame(3, csha2p_response))
        .expect_read(create_more_data_frame(4, fast_auth_ok))
        .expect_read(create_ok_frame(5, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .check(fix);
}

// If we're using a secure transport (e.g. UNIX socket), caching_sha2_password
// just sends the raw password
BOOST_AUTO_TEST_CASE(securetransport_fullauth_ok)
{
    // Setup
    handshake_fixture fix(handshake_params("example_user", "example_password"), true);

    // Run the test
    algo_test()
        .expect_read(server_hello_builder()
                         .caps(min_caps)
                         .auth_plugin("caching_sha2_password")
                         .auth_data(csha2p_challenge)
                         .build())
        .expect_write(login_request_builder()
                          .caps(min_caps)
                          .auth_plugin("caching_sha2_password")
                          .auth_response(csha2p_response)
                          .build())
        .expect_read(create_more_data_frame(2, perform_full_auth))
        .expect_write(create_frame(3, null_terminated_password()))
        .expect_read(create_ok_frame(4, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .check(fix);
}

// Same, but failing
// If we're using a secure transport (e.g. UNIX socket), caching_sha2_password
// just sends the raw password
BOOST_AUTO_TEST_CASE(securetransport_fullauth_err)
{
    // Setup
    handshake_fixture fix(true);

    // Run the test
    algo_test()
        .expect_read(
            server_hello_builder().auth_plugin("caching_sha2_password").auth_data(csha2p_challenge).build()
        )
        .expect_write(login_request_builder()
                          .auth_plugin("caching_sha2_password")
                          .auth_response(csha2p_response)
                          .build())
        .expect_read(create_more_data_frame(2, perform_full_auth))
        .expect_write(create_frame(3, null_terminated_password()))
        .expect_read(err_builder()
                         .seqnum(4)
                         .code(common_server_errc::er_access_denied_error)
                         .message("Denied")
                         .build_frame())
        .will_set_capabilities(min_caps)
        .will_set_connection_id(42)
        .check(fix, common_server_errc::er_access_denied_error, create_server_diag("Denied"));
}

// TODO: tls authswitch fullauth ok

// TODO: securetransport fullauth fastok
// TODO: securetransport fullauth unknown_more_data
// TODO: securetransport fullauth authswitch

// Spotcheck: TLS counts as a secure channel
BOOST_AUTO_TEST_CASE(tls)
{
    // Setup
    handshake_fixture fix;
    fix.st.tls_supported = true;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder()
                         .caps(tls_caps)
                         .auth_plugin("caching_sha2_password")
                         .auth_data(csha2p_challenge)
                         .build())
        .expect_write(create_ssl_request())
        .expect_ssl_handshake()
        .expect_write(login_request_builder()
                          .seqnum(2)
                          .caps(tls_caps)
                          .auth_plugin("caching_sha2_password")
                          .auth_response(csha2p_response)
                          .build())
        .expect_read(create_more_data_frame(3, perform_full_auth))
        .expect_write(create_frame(4, null_terminated_password()))
        .expect_read(create_ok_frame(5, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(tls_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .will_set_tls_active(true)
        .check(fix);
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace
