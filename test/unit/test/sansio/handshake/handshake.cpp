//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/error_categories.hpp>
#include <boost/mysql/mariadb_server_errc.hpp>

#include "handshake_common.hpp"
#include "test_common/create_diagnostics.hpp"
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
// Errors processing server hello
//

// The initial hello is invalid
BOOST_AUTO_TEST_CASE(hello_deserialize_error)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(create_frame(0, {0x09, 0x00}))  // unsupported v9 protocol
        .check(fix, client_errc::server_unsupported);
}

// The authentication plugin reports an error while hashing the password
// with the data in the initial hello
BOOST_AUTO_TEST_CASE(hello_hash_password_error)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().auth_data(std::vector<std::uint8_t>(21, 0x0a)).build())
        .check(fix, client_errc::protocol_value_error);
}

//
// Errors processing the initial server response
//

// Deserialization happens with the correct db_flavor
BOOST_AUTO_TEST_CASE(initial_response_err_flavor)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().auth_data(mnp_challenge).version("11.4.2-MariaDB-ubu2404").build()
        )
        .expect_write(login_request_builder().auth_response(mnp_response).build())
        .expect_read(
            err_builder().seqnum(2).code(mariadb_server_errc::er_bad_data).message("bad data").build_frame()
        )
        .check(
            fix,
            error_code(mariadb_server_errc::er_bad_data, get_mariadb_server_category()),
            create_server_diag("bad data")
        );
}

//
// Errors processing the auth switch
//

// TODO: move to generic section
BOOST_AUTO_TEST_CASE(authswitch_error)
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
        .check(fix, common_server_errc::er_access_denied_error, create_server_diag("Denied"));
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
        .expect_read(create_more_data_frame(2, csha2p_fast_auth_ok))
        .expect_read(create_auth_switch_frame(3, "mysql_native_password", mnp_challenge))
        .check(fix, client_errc::bad_handshake_packet_type);
}

//
// mysql_native_password
//

// TODO: keep this?
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
        .check(fix, client_errc::protocol_value_error);
}

// TODO: redo this as a generic test
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
        .check(fix, client_errc::unknown_auth_plugin);
}

// TODO: auth switch to itself (after https://github.com/boostorg/mysql/issues/469)
// TODO: auth switch more than once (after https://github.com/boostorg/mysql/issues/469)

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
        .check(fix, client_errc::sequence_number_mismatch);
}

// TODO: the adequate db_flavor is passed when deserializing errors

BOOST_AUTO_TEST_SUITE_END()

}  // namespace
