//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/client_errc.hpp>

#include <boost/core/span.hpp>

#include <cstring>
#include <vector>

#include "handshake_common.hpp"
#include "handshake_csha2p_keys.hpp"
#include "test_common/create_diagnostics.hpp"
#include "test_unit/create_err.hpp"
#include "test_unit/create_frame.hpp"
#include "test_unit/create_ok.hpp"
#include "test_unit/create_ok_frame.hpp"

using namespace boost::mysql::test;
using namespace boost::mysql;
using detail::capabilities;
using detail::connection_status;

namespace {

BOOST_AUTO_TEST_SUITE(test_handshake_csha2p)

// Edge case: we tolerate a direct OK packet in the fast path, without a fast auth OK
BOOST_AUTO_TEST_CASE(ok)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(
            server_hello_builder().auth_plugin("caching_sha2_password").auth_data(csha2p_scramble).build()
        )
        .expect_write(
            login_request_builder().auth_plugin("caching_sha2_password").auth_response(csha2p_hash).build()
        )
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
            server_hello_builder().auth_plugin("caching_sha2_password").auth_data(csha2p_scramble).build()
        )
        .expect_write(
            login_request_builder().auth_plugin("caching_sha2_password").auth_response(csha2p_hash).build()
        )
        .expect_read(err_builder()
                         .seqnum(2)
                         .code(common_server_errc::er_access_denied_error)
                         .message("Denied")
                         .build_frame())
        .check(fix, common_server_errc::er_access_denied_error, create_server_diag("Denied"));
}

// Receiving an unknown more data frame (something != fullauth or fastok) is illegal
BOOST_AUTO_TEST_CASE(moredata)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(
            server_hello_builder().auth_plugin("caching_sha2_password").auth_data(csha2p_scramble).build()
        )
        .expect_write(
            login_request_builder().auth_plugin("caching_sha2_password").auth_response(csha2p_hash).build()
        )
        .expect_read(create_more_data_frame(2, std::vector<std::uint8_t>{3, 4}))
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
            server_hello_builder().auth_plugin("caching_sha2_password").auth_data(csha2p_scramble).build()
        )
        .expect_write(
            login_request_builder().auth_plugin("caching_sha2_password").auth_response(csha2p_hash).build()
        )
        .expect_read(create_more_data_frame(2, csha2p_fast_auth_ok))
        .expect_read(create_ok_frame(3, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .check(fix);
}

// Password errors don't trigger this path (they always go through full auth),
// but other errors (like incorrect database) trigger this path
BOOST_AUTO_TEST_CASE(fastok_err)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(
            server_hello_builder().auth_plugin("caching_sha2_password").auth_data(csha2p_scramble).build()
        )
        .expect_write(
            login_request_builder().auth_plugin("caching_sha2_password").auth_response(csha2p_hash).build()
        )
        .expect_read(create_more_data_frame(2, csha2p_fast_auth_ok))
        .expect_read(err_builder()
                         .seqnum(3)
                         .code(common_server_errc::er_access_denied_error)
                         .message("Denied")
                         .build_frame())
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
            server_hello_builder().auth_plugin("caching_sha2_password").auth_data(csha2p_scramble).build()
        )
        .expect_write(
            login_request_builder().auth_plugin("caching_sha2_password").auth_response(csha2p_hash).build()
        )
        .expect_read(create_more_data_frame(2, csha2p_fast_auth_ok))
        .expect_read(create_more_data_frame(3, csha2p_fast_auth_ok))
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
            server_hello_builder().auth_plugin("caching_sha2_password").auth_data(csha2p_scramble).build()
        )
        .expect_write(
            login_request_builder().auth_plugin("caching_sha2_password").auth_response(csha2p_hash).build()
        )
        .expect_read(create_more_data_frame(2, csha2p_fast_auth_ok))
        .expect_read(create_more_data_frame(3, csha2p_perform_full_auth))
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
            server_hello_builder().auth_plugin("caching_sha2_password").auth_data(csha2p_scramble).build()
        )
        .expect_write(
            login_request_builder().auth_plugin("caching_sha2_password").auth_response(csha2p_hash).build()
        )
        .expect_read(create_more_data_frame(2, csha2p_fast_auth_ok))
        .expect_read(create_more_data_frame(3, std::vector<std::uint8_t>{10, 20, 30}))
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
            server_hello_builder().auth_plugin("mysql_native_password").auth_data(mnp_scramble).build()
        )
        .expect_write(
            login_request_builder().auth_plugin("mysql_native_password").auth_response(mnp_hash).build()
        )
        .expect_read(create_auth_switch_frame(2, "caching_sha2_password", csha2p_scramble))
        .expect_write(create_frame(3, csha2p_hash))
        .expect_read(create_more_data_frame(4, csha2p_fast_auth_ok))
        .expect_read(create_ok_frame(5, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .check(fix);
}

// If the server requests us to perform full auth and we're using plaintext,
// we request the server's public key
BOOST_AUTO_TEST_CASE(fullauth_key_ok)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder()
                         .caps(tls_caps)
                         .auth_plugin("caching_sha2_password")
                         .auth_data(csha2p_scramble)
                         .build())
        .expect_write(
            login_request_builder().auth_plugin("caching_sha2_password").auth_response(csha2p_hash).build()
        )
        .expect_read(create_more_data_frame(2, csha2p_perform_full_auth))
        .expect_write(create_frame(3, csha2p_request_key))
        .expect_read(create_more_data_frame(4, public_key_2048))
        .expect_any_write()  // the exact encryption result is not deterministic
        .expect_read(create_ok_frame(6, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .check(fix);
}

// The server might send us an error after we sent the password (e.g. unauthorized)
BOOST_AUTO_TEST_CASE(fullauth_key_error)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder()
                         .caps(tls_caps)
                         .auth_plugin("caching_sha2_password")
                         .auth_data(csha2p_scramble)
                         .build())
        .expect_write(
            login_request_builder().auth_plugin("caching_sha2_password").auth_response(csha2p_hash).build()
        )
        .expect_read(create_more_data_frame(2, csha2p_perform_full_auth))
        .expect_write(create_frame(3, csha2p_request_key))
        .expect_read(create_more_data_frame(4, public_key_2048))
        .expect_any_write()  // the exact encryption result is not deterministic
        .expect_read(err_builder()
                         .seqnum(6)
                         .code(common_server_errc::er_access_denied_error)
                         .message("Denied")
                         .build_frame())
        .check(fix, common_server_errc::er_access_denied_error, create_server_diag("Denied"));
}

// The server might send us an error instead of the key,
// if the key pair for caching_sha2_password was misconfigured
BOOST_AUTO_TEST_CASE(fullauth_error)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder()
                         .caps(tls_caps)
                         .auth_plugin("caching_sha2_password")
                         .auth_data(csha2p_scramble)
                         .build())
        .expect_write(
            login_request_builder().auth_plugin("caching_sha2_password").auth_response(csha2p_hash).build()
        )
        .expect_read(create_more_data_frame(2, csha2p_perform_full_auth))
        .expect_write(create_frame(3, csha2p_request_key))
        .expect_read(err_builder()
                         .seqnum(4)
                         .code(common_server_errc::er_access_denied_error)
                         .message("Bad key")
                         .build_frame())
        .check(fix, common_server_errc::er_access_denied_error, create_server_diag("Bad key"));
}

// If encryption fails (e.g. because the server sent us an invalid key), we fail appropriately.
// Size checks are the only ones that yield a predictable error code
BOOST_AUTO_TEST_CASE(fullauth_encrypterror)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder()
                         .caps(tls_caps)
                         .auth_plugin("caching_sha2_password")
                         .auth_data(csha2p_scramble)
                         .build())
        .expect_write(
            login_request_builder().auth_plugin("caching_sha2_password").auth_response(csha2p_hash).build()
        )
        .expect_read(create_more_data_frame(2, csha2p_perform_full_auth))
        .expect_write(create_frame(3, csha2p_request_key))
        .expect_read(create_more_data_frame(4, std::vector<std::uint8_t>(2u * 1024u * 1024u, 0xab)))
        .check(fix, client_errc::protocol_value_error);
}

// After encrypting the password, no more messages are expected
BOOST_AUTO_TEST_CASE(fullauth_key_moredata)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder()
                         .caps(tls_caps)
                         .auth_plugin("caching_sha2_password")
                         .auth_data(csha2p_scramble)
                         .build())
        .expect_write(
            login_request_builder().auth_plugin("caching_sha2_password").auth_response(csha2p_hash).build()
        )
        .expect_read(create_more_data_frame(2, csha2p_perform_full_auth))
        .expect_write(create_frame(3, csha2p_request_key))
        .expect_read(create_more_data_frame(4, public_key_2048))
        .expect_any_write()  // the exact encryption result is not deterministic
        .expect_read(create_more_data_frame(6, csha2p_perform_full_auth))
        .check(fix, client_errc::bad_handshake_packet_type);
}

// If we're using a secure transport (e.g. UNIX socket), caching_sha2_password
// just sends the raw password
BOOST_AUTO_TEST_CASE(securetransport_fullauth_ok)
{
    // Setup
    handshake_fixture fix(true);

    // Run the test
    algo_test()
        .expect_read(
            server_hello_builder().auth_plugin("caching_sha2_password").auth_data(csha2p_scramble).build()
        )
        .expect_write(
            login_request_builder().auth_plugin("caching_sha2_password").auth_response(csha2p_hash).build()
        )
        .expect_read(create_more_data_frame(2, csha2p_perform_full_auth))
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
            server_hello_builder().auth_plugin("caching_sha2_password").auth_data(csha2p_scramble).build()
        )
        .expect_write(
            login_request_builder().auth_plugin("caching_sha2_password").auth_response(csha2p_hash).build()
        )
        .expect_read(create_more_data_frame(2, csha2p_perform_full_auth))
        .expect_write(create_frame(3, null_terminated_password()))
        .expect_read(err_builder()
                         .seqnum(4)
                         .code(common_server_errc::er_access_denied_error)
                         .message("Denied")
                         .build_frame())
        .check(fix, common_server_errc::er_access_denied_error, create_server_diag("Denied"));
}

// A fast track OK after a fullauth request is not legal
BOOST_AUTO_TEST_CASE(securetransport_fullauth_fastok)
{
    // Setup
    handshake_fixture fix(true);

    // Run the test
    algo_test()
        .expect_read(
            server_hello_builder().auth_plugin("caching_sha2_password").auth_data(csha2p_scramble).build()
        )
        .expect_write(
            login_request_builder().auth_plugin("caching_sha2_password").auth_response(csha2p_hash).build()
        )
        .expect_read(create_more_data_frame(2, csha2p_perform_full_auth))
        .expect_write(create_frame(3, null_terminated_password()))
        .expect_read(create_more_data_frame(4, csha2p_fast_auth_ok))
        .check(fix, client_errc::bad_handshake_packet_type);
}

// Two consecutive full auth requests are not legal
BOOST_AUTO_TEST_CASE(securetransport_fullauth_fullauth)
{
    // Setup
    handshake_fixture fix(true);

    // Run the test
    algo_test()
        .expect_read(
            server_hello_builder().auth_plugin("caching_sha2_password").auth_data(csha2p_scramble).build()
        )
        .expect_write(
            login_request_builder().auth_plugin("caching_sha2_password").auth_response(csha2p_hash).build()
        )
        .expect_read(create_more_data_frame(2, csha2p_perform_full_auth))
        .expect_write(create_frame(3, null_terminated_password()))
        .expect_read(create_more_data_frame(4, csha2p_perform_full_auth))
        .check(fix, client_errc::bad_handshake_packet_type);
}

// Any subsequent more data frames are illegal
BOOST_AUTO_TEST_CASE(securetransport_fullauth_moredata)
{
    // Setup
    handshake_fixture fix(true);

    // Run the test
    algo_test()
        .expect_read(
            server_hello_builder().auth_plugin("caching_sha2_password").auth_data(csha2p_scramble).build()
        )
        .expect_write(
            login_request_builder().auth_plugin("caching_sha2_password").auth_response(csha2p_hash).build()
        )
        .expect_read(create_more_data_frame(2, csha2p_perform_full_auth))
        .expect_write(create_frame(3, null_terminated_password()))
        .expect_read(create_more_data_frame(4, std::vector<std::uint8_t>{4, 3, 2}))
        .check(fix, client_errc::bad_handshake_packet_type);
}

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
                         .auth_data(csha2p_scramble)
                         .build())
        .expect_write(create_ssl_request())
        .expect_ssl_handshake()
        .expect_write(login_request_builder()
                          .seqnum(2)
                          .caps(tls_caps)
                          .auth_plugin("caching_sha2_password")
                          .auth_response(csha2p_hash)
                          .build())
        .expect_read(create_more_data_frame(3, csha2p_perform_full_auth))
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
