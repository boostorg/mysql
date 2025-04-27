//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/character_set.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/common_server_errc.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/metadata_mode.hpp>
#include <boost/mysql/mysql_collations.hpp>
#include <boost/mysql/ssl_mode.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/character_set.hpp>
#include <boost/mysql/detail/execution_processor/execution_processor.hpp>

#include <boost/mysql/impl/internal/protocol/capabilities.hpp>
#include <boost/mysql/impl/internal/protocol/db_flavor.hpp>
#include <boost/mysql/impl/internal/protocol/frame_header.hpp>
#include <boost/mysql/impl/internal/protocol/impl/protocol_types.hpp>
#include <boost/mysql/impl/internal/protocol/impl/serialization_context.hpp>
#include <boost/mysql/impl/internal/protocol/serialization.hpp>
#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>
#include <boost/mysql/impl/internal/sansio/handshake.hpp>

#include <boost/core/span.hpp>
#include <boost/endian/conversion.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_suite.hpp>

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

#include "handshake/handshake_common.hpp"
#include "test_common/create_diagnostics.hpp"
#include "test_common/printing.hpp"
#include "test_unit/algo_test.hpp"
#include "test_unit/create_err.hpp"
#include "test_unit/create_frame.hpp"
#include "test_unit/create_ok.hpp"
#include "test_unit/create_ok_frame.hpp"
#include "test_unit/printing.hpp"

using namespace boost::mysql::test;
using namespace boost::mysql;
using detail::capabilities;
using detail::connection_status;

namespace {

BOOST_AUTO_TEST_SUITE(test_handshake)

//
// mysql_native_password
//

BOOST_AUTO_TEST_CASE(mnp_ok)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().auth_data(mnp_challenge).build())
        .expect_write(login_request_builder().auth_response(mnp_response).build())
        .expect_read(create_ok_frame(2, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .check(fix);
}

BOOST_AUTO_TEST_CASE(mnp_err)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().auth_data(mnp_challenge).build())
        .expect_write(login_request_builder().auth_response(mnp_response).build())
        .expect_read(err_builder()
                         .seqnum(2)
                         .code(common_server_errc::er_access_denied_error)
                         .message("Denied")
                         .build_frame())
        .will_set_capabilities(min_caps)  // incidental
        .will_set_connection_id(42)       // incidental
        .check(fix, common_server_errc::er_access_denied_error, create_server_diag("Denied"));
}

// The authentication plugin generates an error during fast track
BOOST_AUTO_TEST_CASE(mnp_bad_challenge_length)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().auth_data(std::vector<std::uint8_t>(21, 0x0a)).build())
        .will_set_capabilities(min_caps)  // incidental
        .will_set_connection_id(42)       // incidental
        .check(fix, client_errc::protocol_value_error);
}

// Receiving a more data message at this point is illegal
// TODO: re-enable this test after https://github.com/boostorg/mysql/issues/469
// BOOST_AUTO_TEST_CASE(mnp_moredata)
// {
//     // Setup
//     fixture fix;

//     // Run the test
//     algo_test()
//         .expect_read(server_hello_builder().auth_data(mnp_challenge).build())
//         .expect_write(login_request_builder().auth_response(mnp_response).build())
//         .expect_read(create_more_data_frame(2, mnp_challenge))
//         .will_set_capabilities(min_caps)  // incidental
//         .will_set_connection_id(42)       // incidental
//         .check(fix, client_errc::protocol_value_error);
// }

BOOST_AUTO_TEST_CASE(mnp_authswitch_ok)
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
        .expect_read(create_auth_switch_frame(2, "mysql_native_password", mnp_challenge))
        .expect_write(create_frame(3, mnp_response))
        .expect_read(create_ok_frame(4, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .check(fix);
}

BOOST_AUTO_TEST_CASE(mnp_authswitch_error)
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
        .expect_read(create_auth_switch_frame(2, "mysql_native_password", mnp_challenge))
        .expect_write(create_frame(3, mnp_response))
        .expect_read(err_builder()
                         .seqnum(4)
                         .code(common_server_errc::er_access_denied_error)
                         .message("Denied")
                         .build_frame())
        .will_set_capabilities(min_caps)  // incidental
        .will_set_connection_id(42)       // incidental
        .check(fix, common_server_errc::er_access_denied_error, create_server_diag("Denied"));
}

// The authentication plugin generates an error during auth switch
BOOST_AUTO_TEST_CASE(mnp_authswitch_bad_challenge_length)
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
        .expect_read(create_auth_switch_frame(2, "mysql_native_password", std::vector<std::uint8_t>(21, 0x0a))
        )
        .will_set_capabilities(min_caps)  // incidental
        .will_set_connection_id(42)       // incidental
        .check(fix, client_errc::protocol_value_error);
}

// After receiving an auth switch, receiving another one is illegal
// TODO: re-enable this test after https://github.com/boostorg/mysql/issues/469
// BOOST_AUTO_TEST_CASE(mnp_authswitch_authswitch)
// {
//     // Setup
//     fixture fix;

//     // Run the test
//     algo_test()
//         .expect_read(
//             server_hello_builder().auth_plugin("caching_sha2_password").auth_data(csha2p_challenge).build()
//         )
//         .expect_write(login_request_builder()
//                           .auth_plugin("caching_sha2_password")
//                           .auth_response(csha2p_response)
//                           .build())
//         .expect_read(create_auth_switch_frame(2, "mysql_native_password", mnp_challenge))
//         .expect_write(create_frame(3, mnp_response))
//         .expect_read(create_auth_switch_frame(4, "mysql_native_password", mnp_challenge))
//         .will_set_capabilities(min_caps)  // incidental
//         .will_set_connection_id(42)       // incidental
//         .check(fix, client_errc::protocol_value_error);
// }

// In mysql_native_password, more data packets are not supported
// TODO: re-enable this test after https://github.com/boostorg/mysql/issues/469
// BOOST_AUTO_TEST_CASE(mnp_authswitch_moredata)
// {
//     // Setup
//     fixture fix;

//     // Run the test
//     algo_test()
//         .expect_read(
//             server_hello_builder().auth_plugin("caching_sha2_password").auth_data(csha2p_challenge).build()
//         )
//         .expect_write(login_request_builder()
//                           .auth_plugin("caching_sha2_password")
//                           .auth_response(csha2p_response)
//                           .build())
//         .expect_read(create_auth_switch_frame(2, "mysql_native_password", mnp_challenge))
//         .expect_write(create_frame(3, mnp_response))
//         .expect_read(create_more_data_frame(4, mnp_challenge))
//         .will_set_capabilities(min_caps)  // incidental
//         .will_set_connection_id(42)       // incidental
//         .check(fix, client_errc::protocol_value_error);
// }

// Spotcheck: mysql_native_password doesn't have interactions with TLS
BOOST_AUTO_TEST_CASE(mnp_tls)
{
    // Setup
    handshake_fixture fix;
    fix.st.tls_supported = true;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().caps(tls_caps).auth_data(mnp_challenge).build())
        .expect_write(create_ssl_request())
        .expect_ssl_handshake()
        .expect_write(login_request_builder().seqnum(2).caps(tls_caps).auth_response(mnp_response).build())
        .expect_read(create_ok_frame(3, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_tls_active(true)
        .will_set_capabilities(tls_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .check(fix);
}

//
// caching_sha2_password
//

static constexpr std::array<std::uint8_t, 1> csha2p_ok_follows{{0x03}};
static constexpr std::array<std::uint8_t, 1> csha2p_full_auth{{0x04}};

// Null-terminated password, as required by the plugin
boost::span<const std::uint8_t> null_terminated_password()
{
    const char* passwd = "example_password";
    return {reinterpret_cast<const std::uint8_t*>(passwd), std::strlen(passwd) + 1};
}

// Edge case: we tolerate a direct OK packet, without an OK follows
BOOST_AUTO_TEST_CASE(csha2p_ok)
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

// Edge case: we tolerate a direct error packet, without an OK follows
BOOST_AUTO_TEST_CASE(csha2p_err)
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
BOOST_AUTO_TEST_CASE(csha2p_fullauth)
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
        .expect_read(create_more_data_frame(2, csha2p_full_auth))
        .will_set_capabilities(min_caps)
        .will_set_connection_id(42)
        .check(fix, client_errc::auth_plugin_requires_ssl);
}

// Usual success when using the fast track
BOOST_AUTO_TEST_CASE(csha2p_okfollows_ok)
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
        .expect_read(create_more_data_frame(2, csha2p_ok_follows))
        .expect_read(create_ok_frame(3, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .check(fix);
}

// Password errors don't trigger this path (they always go through full auth),
// but other errors (like incorrect database) trigger this path
// TODO: uncomment this after https://github.com/boostorg/mysql/issues/468 gets fixed
// BOOST_AUTO_TEST_CASE(csha2p_okfollows_err)
// {
//     // Setup
//     fixture fix;

//     // Run the test
//     algo_test()
//         .expect_read(
//             server_hello_builder().auth_plugin("caching_sha2_password").auth_data(csha2p_challenge).build()
//         )
//         .expect_write(login_request_builder()
//                           .auth_plugin("caching_sha2_password")
//                           .auth_response(csha2p_response)
//                           .build())
//         .expect_read(create_more_data_frame(2, csha2p_ok_follows))
//         .expect_read(err_builder()
//                          .seqnum(3)
//                          .code(common_server_errc::er_access_denied_error)
//                          .message("Denied")
//                          .build_frame())
//         .will_set_capabilities(min_caps)  // incidental
//         .will_set_connection_id(42)       // incidental
//         .check(fix, common_server_errc::er_access_denied_error, create_server_diag("Denied"));
// }

// The authentication plugin raises an error during the "fast track" auth
BOOST_AUTO_TEST_CASE(csha2p_bad_challenge_length)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder()
                         .auth_plugin("caching_sha2_password")
                         .auth_data(std::vector<std::uint8_t>(21, 0x01))
                         .build())
        .will_set_capabilities(min_caps)
        .will_set_connection_id(42)
        .check(fix, client_errc::protocol_value_error);
}

// Usual flow when requesting full auth
BOOST_AUTO_TEST_CASE(csha2p_tls_fullauth_ok)
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
        .expect_read(create_more_data_frame(3, csha2p_full_auth))
        .expect_write(create_frame(4, null_terminated_password()))
        .expect_read(create_ok_frame(5, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(tls_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .will_set_tls_active(true)
        .check(fix);
}

// Error with full auth
BOOST_AUTO_TEST_CASE(csha2p_tls_fullauth_err)
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
        .expect_read(create_more_data_frame(3, csha2p_full_auth))
        .expect_write(create_frame(4, null_terminated_password()))
        .expect_read(err_builder()
                         .seqnum(5)
                         .code(common_server_errc::er_access_denied_error)
                         .message("Denied")
                         .build_frame())
        .will_set_capabilities(tls_caps)
        .will_set_connection_id(42)
        .will_set_tls_active(true)
        .check(fix, common_server_errc::er_access_denied_error, create_server_diag("Denied"));
}

BOOST_AUTO_TEST_CASE(csha2p_authswitch_okfollows_ok)
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
        .expect_read(create_more_data_frame(4, csha2p_ok_follows))
        .expect_read(create_ok_frame(5, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .check(fix);
}

// If we're using a secure transport (e.g. UNIX socket), caching_sha2_password
// just sends the raw password
// TODO: after https://github.com/boostorg/mysql/issues/469,
// maybe run more cases with secure_channel=true
BOOST_AUTO_TEST_CASE(csha2p_securetransport_fullauth_ok)
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
        .expect_read(create_more_data_frame(2, csha2p_full_auth))
        .expect_write(create_frame(3, null_terminated_password()))
        .expect_read(create_ok_frame(4, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .check(fix);
}

//
// capabilities: connect with db
//

constexpr auto db_caps = min_caps | capabilities::connect_with_db;

BOOST_AUTO_TEST_CASE(db_nonempty_supported)
{
    // Setup
    handshake_fixture fix(handshake_params("example_user", "example_password", "mydb"));

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().caps(db_caps).auth_data(mnp_challenge).build())
        .expect_write(login_request_builder().caps(db_caps).auth_response(mnp_response).db("mydb").build())
        .expect_read(create_ok_frame(2, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(db_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .check(fix);
}

BOOST_AUTO_TEST_CASE(db_nonempty_unsupported)
{
    // Setup
    handshake_fixture fix(handshake_params("example_user", "example_password", "mydb"));

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().caps(min_caps).auth_data(mnp_challenge).build())
        .check(fix, client_errc::server_unsupported);  // TODO: some diagnostics here would be great
}

// If the user didn't request a DB, we don't send it
BOOST_AUTO_TEST_CASE(db_empty_supported)
{
    // Setup
    handshake_fixture fix(handshake_params("example_user", "example_password", ""));

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().caps(db_caps).auth_data(mnp_challenge).build())
        .expect_write(login_request_builder().caps(min_caps).auth_response(mnp_response).build())
        .expect_read(create_ok_frame(2, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .check(fix);
}

// If the server doesn't support connect with DB but the user didn't request it, we don't fail
BOOST_AUTO_TEST_CASE(db_empty_unsupported)
{
    // Setup
    handshake_fixture fix(handshake_params("example_user", "example_password", ""));

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().auth_data(mnp_challenge).build())
        .expect_write(login_request_builder().auth_response(mnp_response).build())
        .expect_read(create_ok_frame(2, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .check(fix);
}

//
// capabilities: multi_queries
//

constexpr auto multiq_caps = min_caps | capabilities::multi_statements;

// We request it and the server supports it
BOOST_AUTO_TEST_CASE(multiq_true_supported)
{
    // Setup
    handshake_params hparams("example_user", "example_password");
    hparams.set_multi_queries(true);
    handshake_fixture fix(hparams);

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().caps(multiq_caps).auth_data(mnp_challenge).build())
        .expect_write(login_request_builder().caps(multiq_caps).auth_response(mnp_response).build())
        .expect_read(create_ok_frame(2, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(multiq_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .check(fix);
}

// We request is but the serve doesn't support it
BOOST_AUTO_TEST_CASE(multiq_true_unsupported)
{
    // Setup
    handshake_params hparams("example_user", "example_password");
    hparams.set_multi_queries(true);
    handshake_fixture fix(hparams);

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().caps(min_caps).auth_data(mnp_challenge).build())
        .check(fix, client_errc::server_unsupported);  // TODO: some diagnostics here would be great
}

// We don't request it but the server supports it. We request the server to disable it
BOOST_AUTO_TEST_CASE(multiq_false_supported)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().caps(multiq_caps).auth_data(mnp_challenge).build())
        .expect_write(login_request_builder().caps(min_caps).auth_response(mnp_response).build())
        .expect_read(create_ok_frame(2, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .check(fix);
}

// We don't request it and the server doesn't support it, either. That's OK
BOOST_AUTO_TEST_CASE(multiq_false_unsupported)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().caps(min_caps).auth_data(mnp_challenge).build())
        .expect_write(login_request_builder().caps(min_caps).auth_response(mnp_response).build())
        .expect_read(create_ok_frame(2, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .check(fix);
}

//
// capabilities: TLS
//

// Cases where we successfully negotiate the use of TLS
BOOST_AUTO_TEST_CASE(tls_on)
{
    for (auto mode : {ssl_mode::enable, ssl_mode::require})
    {
        BOOST_TEST_CONTEXT(mode)
        {
            // Setup
            handshake_params hparams("example_user", "example_password");
            hparams.set_ssl(mode);
            handshake_fixture fix(hparams, false);  // TLS only negotiated when the transport is not secure
            fix.st.tls_supported = true;            // TLS only negotiated if supported

            // Run the test
            algo_test()
                .expect_read(server_hello_builder().caps(tls_caps).auth_data(mnp_challenge).build())
                .expect_write(create_ssl_request())
                .expect_ssl_handshake()
                .expect_write(
                    login_request_builder().seqnum(2).caps(tls_caps).auth_response(mnp_response).build()
                )
                .expect_read(create_ok_frame(3, ok_builder().build()))
                .will_set_status(connection_status::ready)
                .will_set_tls_active(true)
                .will_set_capabilities(tls_caps)
                .will_set_current_charset(utf8mb4_charset)
                .will_set_connection_id(42)
                .check(fix);
        }
    }
}

// Cases where we negotiate that we won't use any TLS
BOOST_AUTO_TEST_CASE(tls_off)
{
    // TODO: handshake should be in charge of not negotiating
    // TLS when a secure channel is in place. Enable these tests then
    // constexpr struct
    // {
    //     const char* name;
    //     ssl_mode mode;
    //     bool secure_transport;
    //     bool transport_supports_tls;
    //     capabilities server_caps;
    // } test_cases[] = {
    //     {"disable_insecure_clino_serverno",   ssl_mode::disable, false, false, min_caps},
    //     {"disable_insecure_clino_serveryes",  ssl_mode::disable, false, false, tls_caps},
    //     {"disable_insecure_cliyes_serverno",  ssl_mode::disable, false, true,  min_caps},
    //     {"disable_insecure_cliyes_serveryes", ssl_mode::disable, false, true,  tls_caps},
    //     {"disable_secure_clino_serverno",     ssl_mode::disable, true,  false, min_caps},
    //     {"disable_secure_clino_serveryes",    ssl_mode::disable, true,  false, tls_caps},
    //     {"disable_secure_cliyes_serverno",    ssl_mode::disable, true,  true,  min_caps},
    //     {"disable_secure_cliyes_serveryes",   ssl_mode::disable, true,  true,  tls_caps},

    //     {"enable_insecure_clino_serverno",    ssl_mode::enable,  false, false, min_caps},
    //     {"enable_insecure_clino_serveryes",   ssl_mode::enable,  false, false, tls_caps},
    //     {"enable_insecure_cliyes_serverno",   ssl_mode::enable,  false, true,  min_caps},
    //     {"enable_secure_clino_serverno",      ssl_mode::enable,  true,  false, min_caps},
    //     {"enable_secure_clino_serveryes",     ssl_mode::enable,  true,  false, tls_caps},
    //     {"enable_secure_cliyes_serverno",     ssl_mode::enable,  true,  true,  min_caps},
    //     // {"enable_secure_cliyes_serveryes",    ssl_mode::enable,  true,  true,  tls_caps},

    //     {"require_insecure_clino_serverno",   ssl_mode::require, false, false, min_caps},
    //     {"require_insecure_clino_serveryes",  ssl_mode::require, false, false, tls_caps},
    //     {"require_secure_clino_serverno",     ssl_mode::require, true,  false, min_caps},
    //     {"require_secure_clino_serveryes",    ssl_mode::require, true,  false, tls_caps},
    //     {"require_secure_cliyes_serverno",    ssl_mode::require, true,  true,  min_caps},
    //     {"require_secure_cliyes_serveryes",   ssl_mode::require, true,  true,  tls_caps},
    // };

    constexpr struct
    {
        const char* name;
        ssl_mode mode;
        bool transport_supports_tls;
        capabilities server_caps;
    } test_cases[] = {
        {"disable_clino_serverno",   ssl_mode::disable, false, min_caps},
        {"disable_clino_serveryes",  ssl_mode::disable, false, tls_caps},
        {"disable_cliyes_serverno",  ssl_mode::disable, true,  min_caps},
        {"disable_cliyes_serveryes", ssl_mode::disable, true,  tls_caps},

        {"enable_clino_serverno",    ssl_mode::enable,  false, min_caps},
        {"enable_clino_serveryes",   ssl_mode::enable,  false, tls_caps},
        {"enable_cliyes_serverno",   ssl_mode::enable,  true,  min_caps},

        {"require_clino_serverno",   ssl_mode::require, false, min_caps},
        {"require_clino_serveryes",  ssl_mode::require, false, tls_caps},
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            // Setup
            handshake_params hparams("example_user", "example_password");
            hparams.set_ssl(tc.mode);
            handshake_fixture fix(hparams, false);
            fix.st.tls_supported = tc.transport_supports_tls;

            // Run the test
            algo_test()
                .expect_read(server_hello_builder().caps(tc.server_caps).auth_data(mnp_challenge).build())
                .expect_write(login_request_builder().caps(min_caps).auth_response(mnp_response).build())
                .expect_read(create_ok_frame(2, ok_builder().build()))
                .will_set_status(connection_status::ready)
                .will_set_capabilities(min_caps)
                .will_set_current_charset(utf8mb4_charset)
                .will_set_connection_id(42)
                .check(fix);
        }
    }
}

// We strongly want TLS but the server doesn't support it
BOOST_AUTO_TEST_CASE(tls_error_unsupported)
{
    // Setup
    handshake_params hparams("example_user", "example_password");
    hparams.set_ssl(ssl_mode::require);
    handshake_fixture fix(hparams, false);  // This doesn't happen if the transport is already secure
    fix.st.tls_supported = true;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().caps(min_caps).auth_data(mnp_challenge).build())
        .check(fix, client_errc::server_doesnt_support_ssl);
}

//
// Base capabilities
//

// If the server doesn't have these, we can't talk to it
BOOST_AUTO_TEST_CASE(caps_mandatory)
{
    constexpr struct
    {
        const char* name;
        capabilities caps;
    } test_cases[] = {
        {"no_plugin_auth",
         capabilities::protocol_41 | capabilities::plugin_auth_lenenc_data | capabilities::deprecate_eof |
             capabilities::secure_connection                                                            },
        {"no_protocol_41",
         capabilities::plugin_auth | capabilities::plugin_auth_lenenc_data | capabilities::deprecate_eof |
             capabilities::secure_connection                                                            },
        {"no_plugin_auth_lenenc_data",
         capabilities::plugin_auth | capabilities::protocol_41 | capabilities::deprecate_eof |
             capabilities::secure_connection                                                            },
        {"no_deprecate_eof",
         capabilities::plugin_auth | capabilities::protocol_41 | capabilities::plugin_auth_lenenc_data |
             capabilities::secure_connection                                                            },
        {"no_secure_connection",
         capabilities::plugin_auth | capabilities::protocol_41 | capabilities::plugin_auth_lenenc_data |
             capabilities::deprecate_eof                                                                },
        {"several_missing",            capabilities::plugin_auth | capabilities::plugin_auth_lenenc_data},
        {"none",                       capabilities{}                                                   },
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            // Setup
            handshake_fixture fix;

            // Run the test
            algo_test()
                .expect_read(server_hello_builder().caps(tc.caps).auth_data(mnp_challenge).build())
                .check(fix, client_errc::server_unsupported);  // TODO: some diagnostics here would be great
        }
    }
}

// If the server doesn't have them, it's OK (but better if it has them)
BOOST_AUTO_TEST_CASE(caps_optional)
{
    constexpr struct
    {
        const char* name;
        capabilities caps;
    } test_cases[] = {
        {"multi_results",    capabilities::multi_results   },
        {"ps_multi_results", capabilities::ps_multi_results},
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            // Setup
            handshake_fixture fix;

            // Run the test
            algo_test()
                .expect_read(server_hello_builder().caps(min_caps | tc.caps).auth_data(mnp_challenge).build())
                .expect_write(
                    login_request_builder().caps(min_caps | tc.caps).auth_response(mnp_response).build()
                )
                .expect_read(create_ok_frame(2, ok_builder().build()))
                .will_set_status(connection_status::ready)
                .will_set_capabilities(min_caps | tc.caps)
                .will_set_current_charset(utf8mb4_charset)
                .will_set_connection_id(42)
                .check(fix);
        }
    }
}

// We don't understand these capabilities, so we set them to off even if the server supports them
BOOST_AUTO_TEST_CASE(caps_ignored)
{
    constexpr struct
    {
        const char* name;
        capabilities caps;
    } test_cases[] = {
        {"long_password",                capabilities::long_password               },
        {"found_rows",                   capabilities::found_rows                  },
        {"long_flag",                    capabilities::long_flag                   },
        {"no_schema",                    capabilities::no_schema                   },
        {"compress",                     capabilities::compress                    },
        {"odbc",                         capabilities::odbc                        },
        {"local_files",                  capabilities::local_files                 },
        {"ignore_space",                 capabilities::ignore_space                },
        {"interactive",                  capabilities::interactive                 },
        {"ignore_sigpipe",               capabilities::ignore_sigpipe              },
        {"transactions",                 capabilities::transactions                },
        {"reserved",                     capabilities::reserved                    },
        {"connect_attrs",                capabilities::connect_attrs               },
        {"can_handle_expired_passwords", capabilities::can_handle_expired_passwords},
        {"session_track",                capabilities::session_track               },
        {"ssl_verify_server_cert",       capabilities::ssl_verify_server_cert      },
        {"optional_resultset_metadata",  capabilities::optional_resultset_metadata },
        {"remember_options",             capabilities::remember_options            },
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            // Setup
            handshake_fixture fix;

            // Run the test
            algo_test()
                .expect_read(server_hello_builder().caps(min_caps | tc.caps).auth_data(mnp_challenge).build())
                .expect_write(login_request_builder().caps(min_caps).auth_response(mnp_response).build())
                .expect_read(create_ok_frame(2, ok_builder().build()))
                .will_set_status(connection_status::ready)
                .will_set_capabilities(min_caps)
                .will_set_current_charset(utf8mb4_charset)
                .will_set_connection_id(42)
                .check(fix);
        }
    }
}

//
// Generic auth plugin errors
//

BOOST_AUTO_TEST_CASE(hello_unknown_plugin)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().auth_plugin("unknown").auth_data(csha2p_challenge).build())
        .will_set_capabilities(min_caps)
        .will_set_connection_id(42)
        .check(fix, client_errc::unknown_auth_plugin);
}

BOOST_AUTO_TEST_CASE(authswitch_unknown_plugin)
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
        .expect_read(create_auth_switch_frame(2, "unknown", mnp_challenge))
        .will_set_capabilities(min_caps)
        .will_set_connection_id(42)
        .check(fix, client_errc::unknown_auth_plugin);
}

// TODO: auth switch to itself (after https://github.com/boostorg/mysql/issues/469)
// TODO: auth switch more than once (after https://github.com/boostorg/mysql/issues/469)

//
// Data in the initial hello
//

// No value of connection_id causes trouble
BOOST_AUTO_TEST_CASE(hello_connection_id)
{
    for (std::uint32_t value : {0u, 10u, 0xffffffffu})
    {
        BOOST_TEST_CONTEXT(value)
        {
            // Setup
            handshake_fixture fix;

            // Run the test
            algo_test()
                .expect_read(server_hello_builder().connection_id(value).auth_data(mnp_challenge).build())
                .expect_write(login_request_builder().auth_response(mnp_response).build())
                .expect_read(create_ok_frame(2, ok_builder().build()))
                .will_set_status(connection_status::ready)
                .will_set_capabilities(min_caps)
                .will_set_current_charset(utf8mb4_charset)
                .will_set_connection_id(value)
                .check(fix);
        }
    }
}

// Flavor is set, regardless of what we had before
BOOST_AUTO_TEST_CASE(flavor)
{
    struct
    {
        string_view version;
        detail::db_flavor flavor;
    } test_cases[] = {
        {"11.4.2-MariaDB-ubu2404",             detail::db_flavor::mariadb},
        {"8.4.1 MySQL Community Server - GPL", detail::db_flavor::mysql  },
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.flavor)
        {
            // Setup
            handshake_fixture fix;
            fix.st.flavor = static_cast<detail::db_flavor>(0xffff);  // make sure that we set the value

            // Run the test
            algo_test()
                .expect_read(server_hello_builder().version(tc.version).auth_data(mnp_challenge).build())
                .expect_write(login_request_builder().auth_response(mnp_response).build())
                .expect_read(create_ok_frame(2, ok_builder().build()))
                .will_set_status(connection_status::ready)
                .will_set_capabilities(min_caps)
                .will_set_current_charset(utf8mb4_charset)
                .will_set_connection_id(42)
                .will_set_flavor(tc.flavor)
                .check(fix);
        }
    }
}

// The value of character_set is cleared if the collation_id is unknown.
// We don't test all supported collations here because we need to verify
// that all supported servers support them (so it's an integration test).
BOOST_AUTO_TEST_CASE(unknown_collation)
{
    // Setup
    handshake_params hparams("example_user", "example_password");
    hparams.set_connection_collation(mysql_collations::utf8mb4_0900_as_ci);
    handshake_fixture fix(hparams);
    fix.st.current_charset = {"other", detail::next_char_utf8mb4};  // make sure that we set the value

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().auth_data(mnp_challenge).build())
        .expect_write(login_request_builder()
                          .collation(mysql_collations::utf8mb4_0900_as_ci)
                          .auth_response(mnp_response)
                          .build())
        .expect_read(create_ok_frame(2, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(character_set{})
        .will_set_connection_id(42)
        .check(fix);
}

// The value of backslash_escapes in the final OK packet doesn't get ignored
BOOST_AUTO_TEST_CASE(backslash_escapes)
{
    // Setup
    handshake_fixture fix;
    fix.st.backslash_escapes = true;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().auth_data(mnp_challenge).build())
        .expect_write(login_request_builder().auth_response(mnp_response).build())
        .expect_read(create_ok_frame(2, ok_builder().no_backslash_escapes(true).build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_backslash_escapes(false)
        .will_set_connection_id(42)
        .check(fix);
}

// Handshake should not modify the value of metadata mode
BOOST_AUTO_TEST_CASE(meta_mode)
{
    // Setup
    handshake_fixture fix;
    fix.st.meta_mode = metadata_mode::full;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().auth_data(mnp_challenge).build())
        .expect_write(login_request_builder().auth_response(mnp_response).build())
        .expect_read(create_ok_frame(2, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .check(fix);
}

//
// Connection status
//

// On success, set to ready, regardless of the initial value
BOOST_AUTO_TEST_CASE(connection_status_success)
{
    constexpr connection_status all_status[] = {
        connection_status::not_connected,
        connection_status::ready,
        connection_status::engaged_in_multi_function
    };

    for (auto initial_status : all_status)
    {
        BOOST_TEST_CONTEXT(initial_status)
        {
            // Setup
            handshake_fixture fix;
            fix.st.status = initial_status;

            // Run the test
            algo_test()
                .expect_read(server_hello_builder().auth_data(mnp_challenge).build())
                .expect_write(login_request_builder().auth_response(mnp_response).build())
                .expect_read(create_ok_frame(2, ok_builder().build()))
                .will_set_status(connection_status::ready)
                .will_set_capabilities(min_caps)
                .will_set_current_charset(utf8mb4_charset)
                .will_set_connection_id(42)
                .check(fix);
        }
    }
}

// On success, set to not connected, regardless of the initial value
BOOST_AUTO_TEST_CASE(connection_status_error)
{
    constexpr connection_status all_status[] = {
        connection_status::not_connected,
        connection_status::ready,
        connection_status::engaged_in_multi_function
    };

    for (auto initial_status : all_status)
    {
        BOOST_TEST_CONTEXT(initial_status)
        {
            // Setup
            handshake_fixture fix;
            fix.st.status = initial_status;

            // Run the test
            algo_test()
                .expect_read(client_errc::sequence_number_mismatch)
                .will_set_status(connection_status::not_connected)
                .check(fix, client_errc::sequence_number_mismatch);
        }
    }
}

//
// Deserialization errors
//

BOOST_AUTO_TEST_CASE(deserialization_error_hello)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(create_frame(0, boost::span<const std::uint8_t>()))
        .check(fix, client_errc::incomplete_message);
}

BOOST_AUTO_TEST_CASE(deserialization_error_handshake_server_response)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().auth_data(mnp_challenge).build())
        .expect_write(login_request_builder().auth_response(mnp_response).build())
        .expect_read(create_frame(2, boost::span<const std::uint8_t>()))
        .will_set_capabilities(min_caps)
        .will_set_connection_id(42)
        .check(fix, client_errc::incomplete_message);
}

//
// Network errors
//

BOOST_AUTO_TEST_CASE(network_error_hello)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(client_errc::sequence_number_mismatch)
        .check(fix, client_errc::sequence_number_mismatch);
}

BOOST_AUTO_TEST_CASE(network_error_ssl_request)
{
    // Setup
    handshake_fixture fix;
    fix.st.tls_supported = true;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().caps(tls_caps).auth_data(mnp_challenge).build())
        .expect_write(create_ssl_request(), client_errc::sequence_number_mismatch)
        .will_set_capabilities(tls_caps)
        .will_set_connection_id(42)
        .check(fix, client_errc::sequence_number_mismatch);
}

BOOST_AUTO_TEST_CASE(network_error_ssl_handshake)
{
    // Setup
    handshake_fixture fix;
    fix.st.tls_supported = true;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().caps(tls_caps).auth_data(mnp_challenge).build())
        .expect_write(create_ssl_request())
        .expect_ssl_handshake(client_errc::sequence_number_mismatch)
        .will_set_capabilities(tls_caps)
        .will_set_connection_id(42)
        .check(fix, client_errc::sequence_number_mismatch);
}

BOOST_AUTO_TEST_CASE(network_error_login_request)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().caps(min_caps).auth_data(mnp_challenge).build())
        .expect_write(
            login_request_builder().caps(min_caps).auth_response(mnp_response).build(),
            client_errc::sequence_number_mismatch
        )
        .will_set_capabilities(min_caps)
        .will_set_connection_id(42)
        .check(fix, client_errc::sequence_number_mismatch);
}

BOOST_AUTO_TEST_CASE(network_error_auth_switch_response)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().caps(min_caps).auth_data(mnp_challenge).build())
        .expect_write(login_request_builder().caps(min_caps).auth_response(mnp_response).build())
        .expect_read(create_auth_switch_frame(2, "caching_sha2_password", csha2p_challenge))
        .expect_write(create_frame(3, csha2p_response), client_errc::sequence_number_mismatch)
        .will_set_capabilities(min_caps)
        .will_set_connection_id(42)
        .check(fix, client_errc::sequence_number_mismatch);
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace
