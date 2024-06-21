//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/constant_string_view.hpp>
#include <boost/mysql/format_sql.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/test/unit_test.hpp>

#include <array>
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

//
// Different glues
//
BOOST_AUTO_TEST_CASE(glue)
{
    struct
    {
        string_view name;
        constant_string_view glue;
        string_view expected;
    } test_cases[] = {
        {"regular",         " OR ",   "1 OR 2 OR 3"    },
        {"braces",          "{}",     "1{}2{}3"        },
        {"invalid_utf8",    " \xff ", "1 \xff 2 \xff 3"},
        {"escapable_chars", "'`",     "1'`2'`3"        },
        {"empty",           "",       "123"            },
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            std::array<int, 3> arr{
                {1, 2, 3}
            };
            auto fn = [](int v, format_context_base& ctx) { format_sql_to(ctx, "{}", v); };

            BOOST_TEST(format_sql(opts, "{}", sequence(arr, fn, tc.glue)) == tc.expected);
        }
    }
}

/**
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
