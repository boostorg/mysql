//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/constant_string_view.hpp>
#include <boost/mysql/format_sql.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/test/unit_test.hpp>

#include <array>
#include <forward_list>
#include <string>
#include <type_traits>
#include <vector>

#include "format_common.hpp"
#include "test_common/has_ranges.hpp"
#include "test_common/printing.hpp"

#ifdef BOOST_MYSQL_HAS_RANGES
#include <ranges>
#endif

using namespace boost::mysql;
using namespace boost::mysql::test;

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

// Different glues
BOOST_AUTO_TEST_CASE(glue)
{
    struct
    {
        string_view name;
        string_view glue;
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

            BOOST_TEST(format_sql(opts, "{}", sequence(arr, fn, runtime(tc.glue))) == tc.expected);
        }
    }
}

//
// Different range types
//
const auto fmt_as_str = [](int v, format_context_base& ctx) { format_sql_to(ctx, "{}", std::to_string(v)); };

BOOST_AUTO_TEST_CASE(range_c_array)
{
    int arr[] = {1, 4, 2};
    BOOST_TEST(format_sql(opts, single_fmt, sequence(arr, fmt_as_str)) == "SELECT '1', '4', '2';");
}

BOOST_AUTO_TEST_CASE(range_forward_list)
{
    std::forward_list<int> col{1, 4, 2};
    BOOST_TEST(format_sql(opts, single_fmt, sequence(col, fmt_as_str)) == "SELECT '1', '4', '2';");
}

#ifdef BOOST_MYSQL_HAS_RANGES
BOOST_AUTO_TEST_CASE(range_input_iterator)
{
    std::istringstream str("1 4 2");
    std::ranges::subrange subr{std::istream_iterator<int>(str), std::istream_iterator<int>()};
    BOOST_TEST(format_sql(opts, single_fmt, sequence(subr, fmt_as_str)) == "SELECT '1', '4', '2';");
}
#endif

#ifdef BOOST_MYSQL_HAS_RANGES
BOOST_AUTO_TEST_CASE(range_not_common)
{
    // Sentinel type != iterator type
    auto r = std::ranges::views::iota(5) | std::ranges::views::take(3);
    static_assert(!std::is_same_v<decltype(r.begin()), decltype(r.end())>);
    BOOST_TEST(format_sql(opts, single_fmt, sequence(r, fmt_as_str)) == "SELECT '5', '6', '7';");
    BOOST_TEST(format_sql(opts, single_fmt, sequence(std::move(r), fmt_as_str)) == "SELECT '5', '6', '7';");
}

BOOST_AUTO_TEST_CASE(range_not_const)
{
    std::vector<long> values{4, 10, 1, 21};
    auto r = values | std::ranges::views::filter([](long v) { return v >= 10; });
    BOOST_TEST(format_sql(opts, single_fmt, sequence(r, fmt_as_str)) == "SELECT '10', '21';");
}
#endif

BOOST_AUTO_TEST_CASE(range_vector_of_bool)
{
    std::vector<bool> values{true, false};
    auto fn = [](bool v, format_context_base& ctx) { format_sql_to(ctx, "{}", v ? "true" : "false"); };
    BOOST_TEST(format_sql(opts, single_fmt, sequence(values, fn)) == "SELECT 'true', 'false';");
}

// Different number of elements
BOOST_AUTO_TEST_CASE(num_elms)
{
    BOOST_TEST(format_sql(opts, single_fmt, sequence(std::vector<int>{}, fmt_as_str)) == "SELECT ;");
    BOOST_TEST(format_sql(opts, single_fmt, sequence(std::vector<int>{1}, fmt_as_str)) == "SELECT '1';");
}

//
// Error cases
//
BOOST_AUTO_TEST_CASE(error_nonempty_spec)
{
    string_view test_cases[] = {
        "{:i}",
        "{:other}",
        "{::}",
        "{::i}",
        "{:i:}",
        "{:i:i}",
    };

    for (auto fmt : test_cases)
    {
        BOOST_TEST_CONTEXT(fmt)
        {
            BOOST_TEST(
                format_single_error(runtime(fmt), sequence(std::vector<int>{10}, fmt_as_str)) ==
                client_errc::format_string_invalid_specifier
            );
        }
    }
}

BOOST_AUTO_TEST_CASE(error_formatting_element)
{
    auto fn = [](int v, format_context_base& ctx) {
        if (v == 42)
            ctx.add_error(client_errc::cancelled);
        format_sql_to(ctx, "{}", v);
    };
    std::vector<int> col{1, 42, 10};
    BOOST_TEST(format_single_error("{}", sequence(col, fn)) == client_errc::cancelled);
}

BOOST_AUTO_TEST_SUITE_END()
