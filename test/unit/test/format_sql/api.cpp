//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/character_set.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/format_sql.hpp>

#include <boost/static_string/static_string.hpp>
#include <boost/test/unit_test.hpp>

#include <cmath>
#include <cstddef>

#include "format_common.hpp"
#include "test_common/printing.hpp"

//
// Contains spotchecks verifying that the main success and error cases
// work using each of the APIs.
//

using namespace boost::mysql;
using namespace boost::mysql::test;

BOOST_AUTO_TEST_SUITE(test_format_sql_api)

constexpr format_options opts{utf8mb4_charset, true};

//
// format_sql
//
BOOST_AUTO_TEST_CASE(format_sql_success)
{
    constexpr const char* format_str = "SELECT * FROM {} WHERE id = {} OR name = {}";
    auto str = format_sql(format_str, opts, identifier("my`table"), 42, "Joh'n");
    BOOST_TEST(str == R"(SELECT * FROM `my``table` WHERE id = 42 OR name = 'Joh\'n')");
}

BOOST_AUTO_TEST_CASE(format_sql_invalid_args)
{
    // When passed invalid arguments (like strings with invalid UTF-8 or NaNs) we throw
    BOOST_CHECK_EXCEPTION(
        format_sql("SELECT {}", opts, "Invalid\xffUTF8"),
        boost::system::system_error,
        [&](const boost::system::system_error& err) {
            std::string expected_diag =
                "A string passed to a formatting function contains a byte sequence that can't be decoded "
                "with the current character set.";
            BOOST_TEST(err.code() == client_errc::invalid_encoding);
            BOOST_TEST(err.what() == expected_diag);
            return true;
        }
    );
}

BOOST_AUTO_TEST_CASE(format_sql_invalid_format_string)
{
    // When passed an invalid format string, we throw
    BOOST_CHECK_EXCEPTION(
        format_sql("SELECT {not_found}", opts, 42),
        boost::system::system_error,
        [&](const boost::system::system_error& err) {
            std::string expected_diag =
                "A format argument referenced by a format string was not found. Check the number of format "
                "arguments passed and their names. [mysql.client:22]";
            BOOST_TEST(err.code() == client_errc::format_arg_not_found);
            BOOST_TEST(err.what() == expected_diag);
            return true;
        }
    );
}

//
// Formatting using format_context: verify that we can achieve similar results as using format_sql
//

BOOST_AUTO_TEST_CASE(format_context_success)
{
    // Helper
    auto get = [](format_context_base& ctx) { return static_cast<format_context&&>(ctx).get().value(); };

    // Empty
    BOOST_TEST(format_context(opts).get().value() == "");

    // Raw
    BOOST_TEST(get(format_context(opts).append_raw("SELECT 'abc'")) == "SELECT 'abc'");

    // Value
    BOOST_TEST(get(format_context(opts).append_value(42)) == "42");
    BOOST_TEST(get(format_context(opts).append_value("a str'ing")) == "'a str\\'ing'");
    BOOST_TEST(get(format_context(opts).append_value(true)) == "1");
    BOOST_TEST(get(format_context(opts).append_value(identifier("abc`d"))) == "`abc``d`");

    // Custom values work
    BOOST_TEST(get(format_context(opts).append_value(custom::condition{"id", 42})) == "`id`=42");

    // Raw/value combinations
    BOOST_TEST(get(format_context(opts).append_raw("SELECT ").append_value(42)) == "SELECT 42");
    BOOST_TEST(get(format_context(opts).append_value(42).append_raw(" OR 1=1")) == "42 OR 1=1");
    BOOST_TEST(
        get(format_context(opts).append_raw("SELECT ").append_raw("* FROM ").append_value(identifier("myt"))
        ) == "SELECT * FROM `myt`"
    );
    BOOST_TEST(
        get(format_context(opts).append_raw("SELECT ").append_value(42).append_raw(" OR 1=1")) ==
        "SELECT 42 OR 1=1"
    );
    BOOST_TEST(
        get(format_context(opts).append_value(42).append_value(nullptr).append_raw(" OR 1=1")) ==
        "42NULL OR 1=1"
    );
    BOOST_TEST(
        get(format_context(opts)
                .append_raw("SELECT ")
                .append_value(42)
                .append_raw(" UNION SELECT ")
                .append_value(true)
                .append_raw(" UNION SELECT 'abc'")) == "SELECT 42 UNION SELECT 1 UNION SELECT 'abc'"
    );
}

// charset and backslash_slashes options are honored
BOOST_AUTO_TEST_CASE(format_context_charset)
{
    format_options opts_charset{ff_charset, true};

    format_context ctx(opts_charset);
    ctx.append_raw("SELECT '\xff{abc' + ")
        .append_value("abd\xff{}")
        .append_raw(" + ")
        .append_value(identifier("i`d`ent\xff`ifier"));
    BOOST_TEST(std::move(ctx).get().value() == "SELECT '\xff{abc' + 'abd\xff{}' + `i``d``ent\xff`ifier`");
}

BOOST_AUTO_TEST_CASE(format_context_backslashes)
{
    format_options opts_charset{ff_charset, false};

    format_context ctx(opts_charset);
    ctx.append_raw("SELECT ")
        .append_value("ab'cd\"ef")
        .append_raw(" + ")
        .append_value(identifier("identif`ier"));
    BOOST_TEST(std::move(ctx).get().value() == "SELECT 'ab''cd\"ef' + `identif``ier`");
}

BOOST_AUTO_TEST_CASE(format_context_error)
{
    // Helper
    auto get = [](format_context_base& ctx) { return static_cast<format_context&&>(ctx).get().error(); };

    // Just an error
    BOOST_TEST(get(format_context(opts).append_value("bad\xff")) == client_errc::invalid_encoding);

    // Raw/error combinations
    BOOST_TEST(
        get(format_context(opts).append_raw("SELECT ").append_value("bad\xff")) ==
        client_errc::invalid_encoding
    );
    BOOST_TEST(
        get(format_context(opts).append_value("bad\xff").append_raw("SELECT 1")) ==
        client_errc::invalid_encoding
    );
    BOOST_TEST(
        get(format_context(opts).append_raw("SELECT 1").append_value("bad\xff").append_raw("SELECT 1")) ==
        client_errc::invalid_encoding
    );

    // Error/value combinations: we keep errors even after appending correct values
    BOOST_TEST(
        get(format_context(opts).append_value("abc").append_value("bad\xff")) == client_errc::invalid_encoding
    );
    BOOST_TEST(
        get(format_context(opts).append_value("bad\xff").append_value("abc")) == client_errc::invalid_encoding
    );
    BOOST_TEST(
        get(format_context(opts)
                .append_raw("SELECT * FROM ")
                .append_value(identifier("db", "tab", "bad\xff"))
                .append_raw(" WHERE id=")
                .append_value(42)) == client_errc::invalid_encoding
    );

    // We only keep the first error
    BOOST_TEST(
        get(format_context(opts).append_value("bad\xff").append_raw("abc").append_value(HUGE_VAL)) ==
        client_errc::invalid_encoding
    );

    // Spotcheck: invalid floats are diagnosed correctly
    BOOST_TEST(get(format_context(opts).append_value(HUGE_VAL)) == client_errc::unformattable_value);
}

// Spotcheck: we can use string types that are not std::string with format context
BOOST_AUTO_TEST_CASE(format_context_custom_string)
{
    constexpr std::size_t string_size = 128;
    using context_t = basic_format_context<boost::static_string<string_size>>;

    context_t ctx(opts);
    ctx.append_raw("SELECT * FROM ")
        .append_value(identifier("myt`able"))
        .append_raw(" WHERE id = ")
        .append_value(42)
        .append_raw(" OR name = ")
        .append_value("Joh'n");
    BOOST_TEST(
        std::move(ctx).get().value() == R"(SELECT * FROM `myt``able` WHERE id = 42 OR name = 'Joh\'n')"
    );
}

//
// Formatting using format_sql_to
//
BOOST_AUTO_TEST_CASE(format_sql_to_success)
{
    format_context ctx(opts);
    format_sql_to("SELECT * FROM {} WHERE id = {} OR name = {}", ctx, identifier("my`table"), 42, "Joh'n");
    BOOST_TEST(
        std::move(ctx).get().value() == R"(SELECT * FROM `my``table` WHERE id = 42 OR name = 'Joh\'n')"
    );
}

BOOST_AUTO_TEST_CASE(format_sql_to_append)
{
    // The output string is not cleared by format_sql_to, allowing appending
    format_context ctx(opts);
    format_sql_to("SELECT {}", ctx, 42);
    format_sql_to(", {}, {}", ctx, "'John'", "\"Doe\"");
    BOOST_TEST(std::move(ctx).get().value() == R"(SELECT 42, '\'John\'', '\"Doe\"')");
}

BOOST_AUTO_TEST_CASE(format_sql_to_custom_type)
{
    format_context ctx(opts);
    format_sql_to("SELECT {}", ctx, custom::condition{"number", 42});
    BOOST_TEST(std::move(ctx).get().value() == "SELECT `number`=42");
}

BOOST_AUTO_TEST_CASE(format_sql_to_custom_charset)
{
    // The character set is honored by the format string and by format args
    format_context ctx({ff_charset, true});
    format_sql_to("SELECT \xff{ {}", ctx, "Joh\xff'n'");
    BOOST_TEST(std::move(ctx).get().value() == "SELECT \xff{ 'Joh\xff'n\\''");
}

BOOST_AUTO_TEST_CASE(format_sql_to_backslash_escapes)
{
    // The backslash escapes options is honored
    format_context ctx({utf8mb4_charset, false});
    format_sql_to("SELECT {}", ctx, "Joh'n");
    BOOST_TEST(std::move(ctx).get().value() == "SELECT 'Joh''n'");
}

BOOST_AUTO_TEST_CASE(format_sql_to_custom_string)
{
    // We can use format_sql_to with contexts that are not format_context
    constexpr std::size_t string_size = 128;
    using context_t = basic_format_context<boost::static_string<string_size>>;

    context_t ctx(opts);
    format_sql_to("SELECT * FROM {} WHERE id = {} OR name = {}", ctx, identifier("myt`able"), 42, "Joh'n");
    BOOST_TEST(
        std::move(ctx).get().value() == R"(SELECT * FROM `myt``able` WHERE id = 42 OR name = 'Joh\'n')"
    );
}

BOOST_AUTO_TEST_CASE(format_sql_to_invalid_arg)
{
    format_context ctx(opts);
    format_sql_to("SELECT {}, {}", ctx, "Bad\xc5", 42);
    BOOST_TEST(std::move(ctx).get().error() == client_errc::invalid_encoding);
}

BOOST_AUTO_TEST_CASE(format_sql_to_invalid_format_string)
{
    format_context ctx(opts);
    format_sql_to("SELECT {broken", ctx, 42);
    BOOST_TEST(std::move(ctx).get().error() == client_errc::format_string_invalid_syntax);
}

BOOST_AUTO_TEST_SUITE_END()
