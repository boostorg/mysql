//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/defaults.hpp>

#include "handshake_common.hpp"
#include "test_unit/create_ok.hpp"
#include "test_unit/create_ok_frame.hpp"
#include "test_unit/printing.hpp"

using namespace boost::mysql::test;
using namespace boost::mysql;
using detail::capabilities;
using detail::connection_status;

namespace {

BOOST_AUTO_TEST_SUITE(test_handshake_connection_state_data)

// connection_id: no value causes trouble
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
                .expect_read(server_hello_builder().connection_id(value).auth_data(mnp_scramble).build())
                .expect_write(login_request_builder().auth_response(mnp_hash).build())
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
                .expect_read(server_hello_builder().version(tc.version).auth_data(mnp_scramble).build())
                .expect_write(login_request_builder().auth_response(mnp_hash).build())
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
        .expect_read(server_hello_builder().auth_data(mnp_scramble).build())
        .expect_write(
            login_request_builder()
                .collation(mysql_collations::utf8mb4_0900_as_ci)
                .auth_response(mnp_hash)
                .build()
        )
        .expect_read(create_ok_frame(2, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(character_set{})
        .will_set_connection_id(42)
        .check(fix);
}

//
// server_default_charset
//

// A sentinel character set representing the server's default. It uses a name
// that no collation ID maps to, so we can tell it apart from the charset that
// the connection collation would resolve to.
const character_set server_default_cs{"server_default", detail::next_char_utf8mb4};

// If a non-empty server_default_charset is passed, it is set in the state data
BOOST_AUTO_TEST_CASE(server_default_charset_set)
{
    // Setup
    handshake_fixture fix(handshake_params("example_user", password), false, server_default_cs);
    fix.st.server_default_charset = {"other", detail::next_char_utf8mb4};  // make sure that we set the value

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().auth_data(mnp_scramble).build())
        .expect_write(login_request_builder().auth_response(mnp_hash).build())
        .expect_read(create_ok_frame(2, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_server_default_charset(server_default_cs)
        .will_set_connection_id(42)
        .check(fix);
}

// If an empty server_default_charset is passed, it is reset in the state data
BOOST_AUTO_TEST_CASE(server_default_charset_empty)
{
    // Setup: pass an empty server_default_charset
    handshake_fixture fix(handshake_params("example_user", password), false, character_set{});
    fix.st.server_default_charset = server_default_cs;  // make sure that we reset the value

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().auth_data(mnp_scramble).build())
        .expect_write(login_request_builder().auth_response(mnp_hash).build())
        .expect_read(create_ok_frame(2, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_server_default_charset(character_set{})
        .will_set_connection_id(42)
        .check(fix);
}

// If a known collation and a non-empty server_default_charset is passed, the known
// collation charset is used (the server default is not consulted)
BOOST_AUTO_TEST_CASE(server_default_charset_known_collation)
{
    // Setup
    handshake_params hparams("example_user", password);
    hparams.set_connection_collation(mysql_collations::ascii_general_ci);
    handshake_fixture fix(hparams, false, server_default_cs);

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().auth_data(mnp_scramble).build())
        .expect_write(
            login_request_builder()
                .collation(mysql_collations::ascii_general_ci)
                .auth_response(mnp_hash)
                .build()
        )
        .expect_read(create_ok_frame(2, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(ascii_charset)
        .will_set_server_default_charset(server_default_cs)
        .will_set_connection_id(42)
        .check(fix);
}

// If an unknown collation and a non-empty server_default_charset is passed, the current
// charset is unknown
BOOST_AUTO_TEST_CASE(server_default_charset_unknown_collation)
{
    // Setup
    handshake_params hparams("example_user", password);
    hparams.set_connection_collation(mysql_collations::utf8mb4_0900_as_ci);
    handshake_fixture fix(hparams, false, server_default_cs);
    fix.st.current_charset = {"other", detail::next_char_utf8mb4};  // make sure that we set the value

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().auth_data(mnp_scramble).build())
        .expect_write(
            login_request_builder()
                .collation(mysql_collations::utf8mb4_0900_as_ci)
                .auth_response(mnp_hash)
                .build()
        )
        .expect_read(create_ok_frame(2, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(character_set{})
        .will_set_server_default_charset(server_default_cs)
        .will_set_connection_id(42)
        .check(fix);
}

// If invalid_connection_id and a non-empty server_default_charset is passed,
// the current charset is the server default
BOOST_AUTO_TEST_CASE(server_default_charset_invalid_collation)
{
    // Setup
    handshake_params hparams("example_user", password);
    hparams.set_connection_collation(invalid_collation_id);
    handshake_fixture fix(hparams, false, server_default_cs);

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().auth_data(mnp_scramble).build())
        .expect_write(login_request_builder().collation(0).auth_response(mnp_hash).build())
        .expect_read(create_ok_frame(2, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(server_default_cs)
        .will_set_server_default_charset(server_default_cs)
        .will_set_connection_id(42)
        .check(fix);
}

// If invalid_collation_id and an empty server_default_charset is passed,
// the current charset is unknown
BOOST_AUTO_TEST_CASE(server_default_charset_invalid_collation_empty)
{
    // Setup
    handshake_params hparams("example_user", password);
    hparams.set_connection_collation(invalid_collation_id);
    handshake_fixture fix(hparams, false, character_set{});
    fix.st.current_charset = {"other", detail::next_char_utf8mb4};  // make sure that we set the value

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().auth_data(mnp_scramble).build())
        .expect_write(login_request_builder().collation(0).auth_response(mnp_hash).build())
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
        .expect_read(server_hello_builder().auth_data(mnp_scramble).build())
        .expect_write(login_request_builder().auth_response(mnp_hash).build())
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
        .expect_read(server_hello_builder().auth_data(mnp_scramble).build())
        .expect_write(login_request_builder().auth_response(mnp_hash).build())
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
                .expect_read(server_hello_builder().auth_data(mnp_scramble).build())
                .expect_write(login_request_builder().auth_response(mnp_hash).build())
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

BOOST_AUTO_TEST_SUITE_END()

}  // namespace
