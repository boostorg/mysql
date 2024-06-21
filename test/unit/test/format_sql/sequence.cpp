//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/format_sql.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/test/unit_test.hpp>

#include <string>
#include <vector>

using namespace boost::mysql;

BOOST_AUTO_TEST_SUITE(test_format_sql_sequence)

constexpr format_options opts{utf8mb4_charset, true};
constexpr const char* single_fmt = "SELECT {};";

//
// Different element types
//
BOOST_AUTO_TEST_CASE(elm_type_formattable)
{
    // We don't invoke the default formatter
    auto fn = [](int v, format_context_base& ctx) { format_sql_to(ctx, "{}", v * v); };
    BOOST_TEST(format_sql(opts, single_fmt, sequence(std::vector<int>{1, 2, 3}, fn)) == "SELECT 1, 4, 9;");
}

BOOST_AUTO_TEST_CASE(elm_type_not_formattable)
{
    struct S
    {
        int v;
    } elms[] = {{1}, {2}, {3}};
    auto fn = [](S v, format_context_base& ctx) { format_sql_to(ctx, "{}", v.v); };
    BOOST_TEST(format_sql(opts, single_fmt, sequence(elms, fn)) == "SELECT 1, 2, 3;");
}

//
// Different function types
//
BOOST_AUTO_TEST_CASE(fn_type_convertible)
{
    std::vector<const char*> elms{"abc", "def"};
    auto fn = [](string_view s, format_context_base& ctx) { format_sql_to(ctx, "{:i}", s); };
    BOOST_TEST(format_sql(opts, single_fmt, sequence(elms, fn)) == "SELECT `abc`, `def`;");
}

BOOST_AUTO_TEST_CASE(fn_decay_copied)
{
    std::vector<long> elms{1, 2};
    auto seq = sequence(elms, [](long v, format_context_base& ctx) {
        format_sql_to(ctx, "{}", std::to_string(v));
    });
    BOOST_TEST(format_sql(opts, single_fmt, seq) == "SELECT '1', '2';");
}

/**
glue
    default
    custom
    custom with {}, invalid chars, escapable chars
range type
    C array
    forward list
    input iterator
    not common
num elms: 0, 1, more
errors
    spec
        {::}
        {:i}
        {::i}
        {:other}
    error formatting elements
 */

BOOST_AUTO_TEST_SUITE_END()
