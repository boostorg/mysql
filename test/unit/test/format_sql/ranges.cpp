//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/blob_view.hpp>
#include <boost/mysql/field.hpp>

#include <boost/config.hpp>
#include <boost/optional/optional.hpp>
#include <boost/test/unit_test.hpp>

#include <chrono>
#include <vector>

#include "format_common.hpp"
#include "test_common/create_basic.hpp"
#include "test_common/printing.hpp"

#ifdef __cpp_lib_string_view
#include <string_view>
#endif
#ifdef __cpp_lib_optional
#include <optional>
#endif

using namespace boost::mysql;
using namespace boost::mysql::test;
using mysql_time = boost::mysql::time;

//
// Verify that the default formatting for ranges works
//
BOOST_AUTO_TEST_SUITE(test_format_sql_ranges)

constexpr format_options opts{utf8mb4_charset, true};
constexpr const char* single_fmt = "SELECT {};";

BOOST_AUTO_TEST_CASE(elm_integral)
{
    // Note: unsigned char is formatted as a blob, instead
    BOOST_TEST(format_sql(opts, single_fmt, std::vector<signed char>{42, -1}) == "SELECT 42, -1;");
    BOOST_TEST(format_sql(opts, single_fmt, std::vector<short>{42, 10}) == "SELECT 42, 10;");
    BOOST_TEST(format_sql(opts, single_fmt, std::vector<unsigned short>{0, 10}) == "SELECT 0, 10;");
    BOOST_TEST(format_sql(opts, single_fmt, std::vector<int>{-1, 8}) == "SELECT -1, 8;");
    BOOST_TEST(format_sql(opts, single_fmt, std::vector<unsigned>{10, 8}) == "SELECT 10, 8;");
    BOOST_TEST(format_sql(opts, single_fmt, std::vector<long>{10, 8}) == "SELECT 10, 8;");
    BOOST_TEST(format_sql(opts, single_fmt, std::vector<unsigned long>{10, 8}) == "SELECT 10, 8;");
    BOOST_TEST(format_sql(opts, single_fmt, std::vector<long long>{10, 8}) == "SELECT 10, 8;");
    BOOST_TEST(format_sql(opts, single_fmt, std::vector<unsigned long long>{10, 8}) == "SELECT 10, 8;");
    BOOST_TEST(format_sql(opts, single_fmt, std::array<bool, 2>{true, false}) == "SELECT 1, 0;");
}

BOOST_AUTO_TEST_CASE(elm_floating_point)
{
    BOOST_TEST(
        format_sql(opts, single_fmt, std::vector<float>{4.2f, 0.0f}) == "SELECT 4.199999809265137e+00, 0e+00;"
    );
    BOOST_TEST(format_sql(opts, single_fmt, std::vector<double>{4.2, 0.0}) == "SELECT 4.2e+00, 0e+00;");
}

BOOST_AUTO_TEST_CASE(elm_string)
{
    BOOST_TEST(
        format_sql(opts, single_fmt, std::vector<const char*>{"abc", "buf"}) == "SELECT 'abc', 'buf';"
    );
    BOOST_TEST(
        format_sql(opts, single_fmt, std::vector<std::string>{"abc", "buf"}) == "SELECT 'abc', 'buf';"
    );
    BOOST_TEST(
        format_sql(opts, single_fmt, std::vector<string_view>{"abc", "buf"}) == "SELECT 'abc', 'buf';"
    );
#ifdef __cpp_lib_string_view
    BOOST_TEST(
        format_sql(opts, single_fmt, std::vector<std::string_view>{"abc", "buf"}) == "SELECT 'abc', 'buf';"
    );
#endif

    // Specifiers handled correctly
    BOOST_TEST(
        format_sql(opts, "FROM {::i};", std::vector<const char*>{"abc", "buf"}) == "FROM `abc`, `buf`;"
    );
    BOOST_TEST(format_sql(opts, "FROM {::r};", std::vector<string_view>{"SELECT", "*"}) == "FROM SELECT, *;");
}

BOOST_AUTO_TEST_CASE(elm_blob)
{
    std::vector<blob> blobs{
        {0x01, 0x00},
        {0xff, 0x2c}
    };
    std::vector<blob_view> blob_views{
        makebv("hello\\"),
        makebv("hello \xc3\xb1!"),
    };

    BOOST_TEST(format_sql(opts, single_fmt, blobs) == "SELECT x'0100', x'ff2c';");
    BOOST_TEST(format_sql(opts, single_fmt, blob_views) == "SELECT x'68656c6c6f5c', x'68656c6c6f20c3b121';");
}

BOOST_AUTO_TEST_CASE(elm_date)
{
    std::vector<date> dates{
        {2021, 1,  20},
        {2020, 10, 1 }
    };
    BOOST_TEST(format_sql(opts, single_fmt, dates) == "SELECT '2021-01-20', '2020-10-01';");
}

BOOST_AUTO_TEST_CASE(elm_datetime)
{
    std::vector<datetime> datetimes{
        {2021, 1, 20, 21, 49, 21, 912},
        {2020, 10, 1, 10, 1, 2}
    };
    BOOST_TEST(
        format_sql(opts, single_fmt, datetimes) ==
        "SELECT '2021-01-20 21:49:21.000912', '2020-10-01 10:01:02.000000';"
    );
}

BOOST_AUTO_TEST_CASE(elm_duration)
{
    std::vector<mysql_time> times{maket(20, 1, 2, 1234), maket(1, 2, 3)};
    std::vector<std::chrono::seconds> secs{std::chrono::seconds(20), std::chrono::seconds(61)};

    BOOST_TEST(format_sql(opts, single_fmt, times) == "SELECT '20:01:02.001234', '01:02:03.000000';");
    BOOST_TEST(format_sql(opts, single_fmt, secs) == "SELECT '00:00:20.000000', '00:01:01.000000';");
}

// TODO: ranges of optionals and fields should not compile
// This would require exposing a type-erased formattable type, as replacement for field_view
BOOST_AUTO_TEST_CASE(elm_field)
{
    std::vector<field> fields{field("abc"), field(42)};
    BOOST_TEST(format_sql(opts, single_fmt, make_fv_vector(10, "42", nullptr)) == "SELECT 10, '42', NULL;");
    BOOST_TEST(format_sql(opts, single_fmt, fields) == "SELECT 'abc', 42;");
}

BOOST_AUTO_TEST_CASE(elm_boost_optional)
{
    std::vector<boost::optional<int>> optionals{42, 10, {}};
    BOOST_TEST(format_sql(opts, single_fmt, optionals) == "SELECT 42, 10, NULL;");
}

#ifdef __cpp_lib_optional
BOOST_AUTO_TEST_CASE(elm_std_optional)
{
    std::vector<std::optional<std::string>> optionals{"abc", {}, "d"};
    BOOST_TEST(format_sql(opts, single_fmt, optionals) == "SELECT 'abc', NULL, 'd';");
}
#endif

BOOST_AUTO_TEST_CASE(elm_custom_type)
{
    std::vector<custom::condition> conds{
        {"f1", 42},
        {"f2", 60},
    };

    BOOST_TEST(format_sql(opts, single_fmt, conds) == "SELECT `f1`=42, `f2`=60;");

    // Specifiers are correctly passed to custom types
    BOOST_TEST(format_sql(opts, "SELECT {::s};", conds) == "SELECT `f1` = 42, `f2` = 60;");
}

/**
Regular ranges
    Range type
        vector, C array, std::array
        test these with u8
        forward_list (forward iterator)
        something with input iterators?
        row, row_view
        vector<bool>
        filter
        not a common range
    Number of elements
        0 elements
        1 elements
        more elements
    Specifiers success
        weird range of string-like
        vector of field_views containing strings
        empty specs OK, does nothing
            :
            ::
        specifiers errors
            :abc
            :abc:
            :abc:i
            ::unknown
        move these to format_strings?
            :abc:{
            :abc:}
    error while formatting an element
    formattable concept
        range of ranges (e.g. rows) isn't formattable
        optional<range> isn't formattable
 */

BOOST_AUTO_TEST_SUITE_END()
