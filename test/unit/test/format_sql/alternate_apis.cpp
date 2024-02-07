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

using namespace boost::mysql;
using namespace boost::mysql::test;

BOOST_AUTO_TEST_SUITE(test_format_sql_alternate_apis)

constexpr format_options opts{utf8mb4_charset, true};

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
        .append_raw(" OR first_name = ")
        .append_value("Joh'n");
    BOOST_TEST(std::move(ctx).get().value() == R"(SELECT * FROM `myt``able` WHERE id = 'Joh\'n')");
}

//
// Formatting using format_sql_to
//

/**
 * success
 * error
 * the given context already has data
 * context that is not format_context
 */

BOOST_AUTO_TEST_SUITE_END()
