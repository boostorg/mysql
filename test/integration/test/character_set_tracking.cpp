//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/character_set.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/mysql_collations.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/ssl_mode.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/assert/source_location.hpp>
#include <boost/test/unit_test.hpp>

#include "test_common/create_basic.hpp"
#include "test_common/network_result.hpp"
#include "test_common/printing.hpp"
#include "test_common/source_location.hpp"
#include "test_integration/any_connection_fixture.hpp"
#include "test_integration/connect_params_builder.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;
using boost::test_tools::per_element;
namespace asio = boost::asio;

BOOST_AUTO_TEST_SUITE(test_character_set_tracking)

static void validate_db_charset(
    any_connection& conn,
    string_view expected_charset,
    boost::source_location loc = BOOST_MYSQL_CURRENT_LOCATION
)
{
    // Issue the query
    constexpr const char*
        query = "SELECT @@character_set_client, @@character_set_connection, @@character_set_results";
    results r;
    conn.async_execute(query, r, as_netresult).validate_no_error(loc);

    // Check
    BOOST_TEST_CONTEXT("Called from " << loc)
    {
        const auto expected = makerows(3, expected_charset, expected_charset, expected_charset);
        BOOST_TEST(r.rows() == expected, per_element());
    }
}

BOOST_FIXTURE_TEST_CASE(charset_lifecycle, any_connection_fixture)
{
    // Non-connected connections have an unknown charset
    BOOST_TEST(conn.current_character_set().error() == client_errc::unknown_character_set);
    BOOST_TEST(conn.format_opts().error() == client_errc::unknown_character_set);

    // Connect with the default character set uses utf8mb4, both in the client
    // and in the server. This double-checks that all supported servers support the
    // collation we use by default.
    connect();
    BOOST_TEST(conn.current_character_set()->name == "utf8mb4");
    BOOST_TEST(conn.format_opts()->charset.name == "utf8mb4");
    validate_db_charset(conn, "utf8mb4");

    // Using set_character_set updates the character set everywhere
    character_set greek_charset{"greek", ascii_charset.next_char};
    conn.async_set_character_set(greek_charset, as_netresult).validate_no_error();
    BOOST_TEST(conn.current_character_set()->name == "greek");
    BOOST_TEST(conn.format_opts()->charset.name == "greek");
    validate_db_charset(conn, "greek");

    // Using reset_connection wipes out client-side character set information
    conn.async_reset_connection(as_netresult).validate_no_error();
    BOOST_TEST(conn.current_character_set().error() == client_errc::unknown_character_set);
    BOOST_TEST(conn.format_opts().error() == client_errc::unknown_character_set);

    // We can use set_character_set to recover from this
    conn.async_set_character_set(greek_charset, as_netresult).validate_no_error();
    BOOST_TEST(conn.current_character_set()->name == "greek");
    BOOST_TEST(conn.format_opts()->charset.name == "greek");
    validate_db_charset(conn, "greek");
}

// For some collations, we set the tracked character set after handshake.
// Check that all the collations that we know are supported by all the servers
// that we support. If the collation is not supported, the server falls back to
// a default charset, so we shouldn't be setting the value of the tracked character set.
BOOST_FIXTURE_TEST_CASE(connect_with_known_collation, io_context_fixture)
{
    constexpr struct
    {
        const char* name;
        std::uint16_t collation_id;
        character_set charset;
    } test_cases[] = {
        {"utf8mb4_bin",        mysql_collations::utf8mb4_bin,        utf8mb4_charset},
        {"utf8mb4_general_ci", mysql_collations::utf8mb4_general_ci, utf8mb4_charset},
        {"ascii_general_ci",   mysql_collations::ascii_general_ci,   ascii_charset  },
        {"ascii_bin",          mysql_collations::ascii_bin,          ascii_charset  },
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            // Setup
            any_connection conn(ctx);

            // Connect
            conn.async_connect(connect_params_builder().collation(tc.collation_id).build(), as_netresult)
                .validate_no_error();

            // Check that the tracked character set and the one chosen by the DB match
            BOOST_TEST(conn.current_character_set().value() == tc.charset);
            validate_db_charset(conn, tc.charset.name);
        }
    }
}

BOOST_FIXTURE_TEST_CASE(connect_with_unknown_collation, any_connection_fixture)
{
    // Connect with a collation that some servers may not support, or that we don't know of
    // utf8mb4_0900_ai_ci is not supported by MariaDB, triggers fallback
    connect(connect_params_builder().collation(mysql_collations::utf8mb4_0900_ai_ci).build());
    BOOST_TEST(conn.current_character_set().error() == client_errc::unknown_character_set);
    BOOST_TEST(conn.format_opts().error() == client_errc::unknown_character_set);

    // Explicitly setting the character set solves the issue
    conn.async_set_character_set(ascii_charset, as_netresult).validate_no_error();
    BOOST_TEST(conn.current_character_set()->name == "ascii");
    validate_db_charset(conn, "ascii");
}

BOOST_AUTO_TEST_SUITE_END()
