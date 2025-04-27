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
        .will_set_capabilities(min_caps)  // incidental
        .will_set_connection_id(42)       // incidental
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
