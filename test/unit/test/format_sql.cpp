//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/blob.hpp>
#include <boost/mysql/character_set.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/format_sql.hpp>
#include <boost/mysql/string_view.hpp>
#include <boost/mysql/time.hpp>

#include <boost/config.hpp>
#include <boost/optional/optional.hpp>
#include <boost/system/system_error.hpp>
#include <boost/test/unit_test.hpp>

#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

#include "test_common/create_basic.hpp"
#include "test_common/printing.hpp"
#include "test_unit/custom_allocator.hpp"

#ifdef __cpp_lib_string_view
#include <string_view>
#endif
#ifdef __cpp_lib_optional
#include <optional>
#endif

using namespace boost::mysql;
using namespace boost::mysql::test;
using mysql_time = boost::mysql::time;
using string_with_alloc = std::basic_string<char, std::char_traits<char>, custom_allocator<char>>;
using blob_with_alloc = std::vector<unsigned char, custom_allocator<unsigned char>>;

// Field with a custom formatter
namespace custom {

struct condition
{
    string_view name;
    int value;
};

}  // namespace custom

namespace boost {
namespace mysql {

template <>
struct formatter<custom::condition>
{
    static void format(const custom::condition& v, format_context_base& ctx)
    {
        ctx.append_value(identifier(v.name)).append_raw("=").append_value(v.value);
    }
};

}  // namespace mysql
}  // namespace boost

BOOST_AUTO_TEST_SUITE(test_format_sql)

//
// formattable concept. We have a type trait (is_formattable_type) for C++ < 20,
// and a concept, for C++ >= 20. This macro checks both without repetition
//
#ifdef BOOST_MYSQL_HAS_CONCEPTS
#define BOOST_MYSQL_CHECK_FORMATTABLE(type, expected)         \
    static_assert(detail::formattable<type> == expected, ""); \
    static_assert(detail::is_formattable_type<type>() == expected, "");
#else
#define BOOST_MYSQL_CHECK_FORMATTABLE(type, expected) \
    static_assert(detail::is_formattable_type<type>() == expected, "");
#endif

// field and field_view accepted (writable fields)
BOOST_MYSQL_CHECK_FORMATTABLE(field_view, true)
BOOST_MYSQL_CHECK_FORMATTABLE(field, true)
BOOST_MYSQL_CHECK_FORMATTABLE(field&, false)
BOOST_MYSQL_CHECK_FORMATTABLE(const field&, false)
BOOST_MYSQL_CHECK_FORMATTABLE(field&&, false)
BOOST_MYSQL_CHECK_FORMATTABLE(const field&&, false)

// Scalars accepted (writable fields)
BOOST_MYSQL_CHECK_FORMATTABLE(std::nullptr_t, true)
BOOST_MYSQL_CHECK_FORMATTABLE(unsigned char, true)
BOOST_MYSQL_CHECK_FORMATTABLE(signed char, true)
BOOST_MYSQL_CHECK_FORMATTABLE(short, true)
BOOST_MYSQL_CHECK_FORMATTABLE(unsigned short, true)
BOOST_MYSQL_CHECK_FORMATTABLE(int, true)
BOOST_MYSQL_CHECK_FORMATTABLE(unsigned int, true)
BOOST_MYSQL_CHECK_FORMATTABLE(long, true)
BOOST_MYSQL_CHECK_FORMATTABLE(unsigned long, true)
BOOST_MYSQL_CHECK_FORMATTABLE(float, true)
BOOST_MYSQL_CHECK_FORMATTABLE(double, true)
BOOST_MYSQL_CHECK_FORMATTABLE(date, true)
BOOST_MYSQL_CHECK_FORMATTABLE(datetime, true)
BOOST_MYSQL_CHECK_FORMATTABLE(mysql_time, true)
BOOST_MYSQL_CHECK_FORMATTABLE(bool, true)
BOOST_MYSQL_CHECK_FORMATTABLE(int&, false)
BOOST_MYSQL_CHECK_FORMATTABLE(bool&, false)

// characters (except signed/unsigned char) not accepted
BOOST_MYSQL_CHECK_FORMATTABLE(char, false)
BOOST_MYSQL_CHECK_FORMATTABLE(wchar_t, false)
BOOST_MYSQL_CHECK_FORMATTABLE(char16_t, false)
BOOST_MYSQL_CHECK_FORMATTABLE(char32_t, false)
#ifdef __cpp_char8_t
BOOST_MYSQL_CHECK_FORMATTABLE(char8_t, false)
#endif
BOOST_MYSQL_CHECK_FORMATTABLE(char&, false)
BOOST_MYSQL_CHECK_FORMATTABLE(const char, false)

// Strings (writable fields)
BOOST_MYSQL_CHECK_FORMATTABLE(std::string, true)
BOOST_MYSQL_CHECK_FORMATTABLE(string_with_alloc, true)
BOOST_MYSQL_CHECK_FORMATTABLE(string_view, true)
#ifdef __cpp_lib_string_view
BOOST_MYSQL_CHECK_FORMATTABLE(std::string_view, true)
#endif
BOOST_MYSQL_CHECK_FORMATTABLE(const char*, true)
BOOST_MYSQL_CHECK_FORMATTABLE(char[16], true)
BOOST_MYSQL_CHECK_FORMATTABLE(std::string&, false)
BOOST_MYSQL_CHECK_FORMATTABLE(std::wstring, false)

// Blobs
BOOST_MYSQL_CHECK_FORMATTABLE(blob, true)
BOOST_MYSQL_CHECK_FORMATTABLE(blob_view, true)
BOOST_MYSQL_CHECK_FORMATTABLE(blob_with_alloc, true)

// optional types accepted (writable fields)
#ifndef BOOST_NO_CXX17_HDR_OPTIONAL
BOOST_MYSQL_CHECK_FORMATTABLE(std::optional<int>, true)
BOOST_MYSQL_CHECK_FORMATTABLE(std::optional<std::string>, true)
BOOST_MYSQL_CHECK_FORMATTABLE(std::optional<int>&, false)
#endif
BOOST_MYSQL_CHECK_FORMATTABLE(boost::optional<string_view>, true)
BOOST_MYSQL_CHECK_FORMATTABLE(boost::optional<blob>, true)
BOOST_MYSQL_CHECK_FORMATTABLE(boost::optional<void*>, false)
BOOST_MYSQL_CHECK_FORMATTABLE(boost::optional<format_options>, false)
BOOST_MYSQL_CHECK_FORMATTABLE(boost::optional<int&>, false)

// identifier accepted
BOOST_MYSQL_CHECK_FORMATTABLE(identifier, true)
BOOST_MYSQL_CHECK_FORMATTABLE(identifier&, false)
BOOST_MYSQL_CHECK_FORMATTABLE(boost::optional<identifier>, false)

// Types with custom formatters accepted, but not references or optionals to them
BOOST_MYSQL_CHECK_FORMATTABLE(custom::condition, true)
BOOST_MYSQL_CHECK_FORMATTABLE(custom::condition&, false)
BOOST_MYSQL_CHECK_FORMATTABLE(const custom::condition&, false)
BOOST_MYSQL_CHECK_FORMATTABLE(custom::condition&&, false)
BOOST_MYSQL_CHECK_FORMATTABLE(const custom::condition&&, false)
BOOST_MYSQL_CHECK_FORMATTABLE(custom::condition*, false)
BOOST_MYSQL_CHECK_FORMATTABLE(boost::optional<custom::condition>, false)

// other stuff not accepted
BOOST_MYSQL_CHECK_FORMATTABLE(void*, false)
BOOST_MYSQL_CHECK_FORMATTABLE(field*, false)
BOOST_MYSQL_CHECK_FORMATTABLE(field_view*, false)
BOOST_MYSQL_CHECK_FORMATTABLE(format_options, false)
BOOST_MYSQL_CHECK_FORMATTABLE(const format_options, false)
BOOST_MYSQL_CHECK_FORMATTABLE(format_options&, false)

//
// Verify that formatting individual values work. This is tested using format_sql
// because it's convenient, but also covers basic_format_context
//

constexpr format_options opts{utf8mb4_charset, true};
constexpr auto single_fmt = "SELECT {};";

BOOST_AUTO_TEST_CASE(individual_null)
{
    // nullptr interpreted as NULL
    BOOST_TEST(format_sql(single_fmt, opts, nullptr) == "SELECT NULL;");
}

BOOST_AUTO_TEST_CASE(individual_signed_char)
{
    BOOST_TEST(format_sql(single_fmt, opts, (signed char)42) == "SELECT 42;");
    BOOST_TEST(format_sql(single_fmt, opts, (signed char)-1) == "SELECT -1;");
    BOOST_TEST(format_sql(single_fmt, opts, (signed char)-128) == "SELECT -128;");
    BOOST_TEST(format_sql(single_fmt, opts, (signed char)127) == "SELECT 127;");
}

BOOST_AUTO_TEST_CASE(individual_unsigned_char)
{
    BOOST_TEST(format_sql(single_fmt, opts, (unsigned char)42) == "SELECT 42;");
    BOOST_TEST(format_sql(single_fmt, opts, (unsigned char)0) == "SELECT 0;");
    BOOST_TEST(format_sql(single_fmt, opts, (unsigned char)0xff) == "SELECT 255;");
}

BOOST_AUTO_TEST_CASE(individual_short)
{
    BOOST_TEST(format_sql(single_fmt, opts, (short)42) == "SELECT 42;");
    BOOST_TEST(format_sql(single_fmt, opts, (short)-1) == "SELECT -1;");
    BOOST_TEST(format_sql(single_fmt, opts, (short)-32768) == "SELECT -32768;");
    BOOST_TEST(format_sql(single_fmt, opts, (short)32767) == "SELECT 32767;");
}

BOOST_AUTO_TEST_CASE(individual_unsigned_short)
{
    BOOST_TEST(format_sql(single_fmt, opts, (unsigned short)42) == "SELECT 42;");
    BOOST_TEST(format_sql(single_fmt, opts, (unsigned short)0) == "SELECT 0;");
    BOOST_TEST(format_sql(single_fmt, opts, (unsigned short)0xffff) == "SELECT 65535;");
}

BOOST_AUTO_TEST_CASE(individual_int)
{
    BOOST_TEST(format_sql(single_fmt, opts, (int)42) == "SELECT 42;");
    BOOST_TEST(format_sql(single_fmt, opts, (int)-1) == "SELECT -1;");
    BOOST_TEST(format_sql(single_fmt, opts, (int)-0x7fffffff - 1) == "SELECT -2147483648;");
    BOOST_TEST(format_sql(single_fmt, opts, (int)0x7fffffff) == "SELECT 2147483647;");
}

BOOST_AUTO_TEST_CASE(individual_unsigned_int)
{
    BOOST_TEST(format_sql(single_fmt, opts, (unsigned int)42) == "SELECT 42;");
    BOOST_TEST(format_sql(single_fmt, opts, (unsigned int)0) == "SELECT 0;");
    BOOST_TEST(format_sql(single_fmt, opts, (unsigned int)0xffffffff) == "SELECT 4294967295;");
}

BOOST_AUTO_TEST_CASE(individual_long)
{
    BOOST_TEST(format_sql(single_fmt, opts, (long)42) == "SELECT 42;");
    BOOST_TEST(format_sql(single_fmt, opts, (long)-1) == "SELECT -1;");
    BOOST_TEST(format_sql(single_fmt, opts, (long)0x7fffffff) == "SELECT 2147483647;");
}

BOOST_AUTO_TEST_CASE(individual_unsigned_long)
{
    BOOST_TEST(format_sql(single_fmt, opts, (unsigned long)42) == "SELECT 42;");
    BOOST_TEST(format_sql(single_fmt, opts, (unsigned long)0) == "SELECT 0;");
    BOOST_TEST(format_sql(single_fmt, opts, (unsigned long)0xffffffff) == "SELECT 4294967295;");
}

BOOST_AUTO_TEST_CASE(individual_long_long)
{
    BOOST_TEST(format_sql(single_fmt, opts, (long long)42) == "SELECT 42;");
    BOOST_TEST(format_sql(single_fmt, opts, (long long)-1) == "SELECT -1;");
    BOOST_TEST(
        format_sql(single_fmt, opts, (long long)(-0x7fffffffffffffff - 1)) == "SELECT -9223372036854775808;"
    );
    BOOST_TEST(format_sql(single_fmt, opts, (long long)0x7fffffffffffffff) == "SELECT 9223372036854775807;");
}

BOOST_AUTO_TEST_CASE(individual_unsigned_long_long)
{
    BOOST_TEST(format_sql(single_fmt, opts, (unsigned long long)42) == "SELECT 42;");
    BOOST_TEST(format_sql(single_fmt, opts, (unsigned long long)0) == "SELECT 0;");
    BOOST_TEST(
        format_sql(single_fmt, opts, (unsigned long long)0xffffffffffffffff) == "SELECT 18446744073709551615;"
    );
}

BOOST_AUTO_TEST_CASE(individual_bool)
{
    BOOST_TEST(format_sql(single_fmt, opts, true) == "SELECT 1;");
    BOOST_TEST(format_sql(single_fmt, opts, false) == "SELECT 0;");
}

BOOST_AUTO_TEST_CASE(individual_float)
{
    // float
    BOOST_TEST(format_sql(single_fmt, opts, 4.2f) == "SELECT 4.199999809265137e+00;");
}

BOOST_AUTO_TEST_CASE(individual_double)
{
    // doubles have many different cases that may cause trouble
    struct
    {
        string_view name;
        double value;
        string_view expected;
    } test_cases[] = {
        {"regular",               4.2,                      "4.2e+00"                 },
        {"regular_precision",     4.298238239237823287327,  "4.298238239237823e+00"   },
        {"exp",                   5.1e+23,                  "5.1e+23"                 },
        {"exp_precision",         4.2982382392378232e+67,   "4.2982382392378234e+67"  },
        {"max",                   1.7976931348623157e+308,  "1.7976931348623157e+308" },
        {"regular_neg",           -4.2,                     "-4.2e+00"                },
        {"regular_precision_neg", -4.298238239237823287327, "-4.298238239237823e+00"  },
        {"exp_neg",               -5.1e+23,                 "-5.1e+23"                },
        {"max_neg",               -1.7976931348623157e+308, "-1.7976931348623157e+308"},
        {"zero",                  0.0,                      "0e+00"                   },
        {"zero_neg",              -0.0,                     "-0e+00"                  },
        {"expneg",                4.2e-12,                  "4.2e-12"                 },
        {"expneg_precision",      4.2872383293922839e-45,   "4.2872383293922836e-45"  },
        {"min",                   2.2250738585072014e-308,  "2.2250738585072014e-308" },
        {"min_neg",               -2.2250738585072014e-308, "-2.2250738585072014e-308"},
        {"denorm",                -4.2872383293922839e-309, "-4.287238329392283e-309" },
        {"min_denorm",            5e-324,                   "5e-324"                  },
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            auto str = format_sql("{}", opts, tc.value);
            BOOST_TEST(str == tc.expected);
        }
    }
}

BOOST_AUTO_TEST_CASE(individual_string_literal)
{
    BOOST_TEST(format_sql(single_fmt, opts, "abc") == "SELECT 'abc';");
    BOOST_TEST(format_sql(single_fmt, opts, "abc'\\ OR 1=1") == "SELECT 'abc\\'\\\\ OR 1=1';");
    BOOST_TEST(format_sql(single_fmt, opts, "hola \xc3\xb1!") == "SELECT 'hola \xc3\xb1!';");
    BOOST_TEST(format_sql(single_fmt, opts, "") == "SELECT '';");
}

BOOST_AUTO_TEST_CASE(individual_c_str)
{
    BOOST_TEST(format_sql(single_fmt, opts, static_cast<const char*>("abc")) == "SELECT 'abc';");
    BOOST_TEST(format_sql(single_fmt, opts, static_cast<const char*>("")) == "SELECT '';");
}

BOOST_AUTO_TEST_CASE(individual_string)
{
    std::string lval = "I'm an lvalue";
    const std::string clval = "I'm const";
    BOOST_TEST(format_sql(single_fmt, opts, lval) == "SELECT 'I\\'m an lvalue';");
    BOOST_TEST(format_sql(single_fmt, opts, clval) == "SELECT 'I\\'m const';");
    BOOST_TEST(format_sql(single_fmt, opts, std::string("abc")) == "SELECT 'abc';");
    BOOST_TEST(format_sql(single_fmt, opts, std::string()) == "SELECT '';");
    BOOST_TEST(format_sql(single_fmt, opts, string_with_alloc("abc'")) == "SELECT 'abc\\'';");
}

BOOST_AUTO_TEST_CASE(individual_string_view)
{
    BOOST_TEST(format_sql(single_fmt, opts, string_view("abc")) == "SELECT 'abc';");
    BOOST_TEST(format_sql(single_fmt, opts, string_view("abc'\\ OR 1=1")) == "SELECT 'abc\\'\\\\ OR 1=1';");
    BOOST_TEST(format_sql(single_fmt, opts, string_view()) == "SELECT '';");
}

#ifdef __cpp_lib_string_view
BOOST_AUTO_TEST_CASE(individual_std_string_view)
{
    BOOST_TEST(format_sql(single_fmt, opts, std::string_view("abc")) == "SELECT 'abc';");
    BOOST_TEST(
        format_sql(single_fmt, opts, std::string_view("abc'\\ OR 1=1")) == "SELECT 'abc\\'\\\\ OR 1=1';"
    );
    BOOST_TEST(format_sql(single_fmt, opts, std::string_view()) == "SELECT '';");
}
#endif

// blob: same semantics as strings
BOOST_AUTO_TEST_CASE(individual_blob)
{
    blob lval{0x68, 0x65, 0x6c, 0x6c, 0x27, 0x6f};  // hell'o
    const blob c_clval = lval;
    BOOST_TEST(format_sql(single_fmt, opts, lval) == "SELECT 'hell\\'o';");
    BOOST_TEST(format_sql(single_fmt, opts, c_clval) == "SELECT 'hell\\'o';");
    BOOST_TEST(format_sql(single_fmt, opts, blob{0x00, 0x01, 0x02}) == "SELECT '\\0\1\2';");
    BOOST_TEST(format_sql(single_fmt, opts, blob()) == "SELECT '';");
    BOOST_TEST(format_sql(single_fmt, opts, blob_with_alloc{0x00, 0x01, 0x02}) == "SELECT '\\0\1\2';");
}

BOOST_AUTO_TEST_CASE(individual_blob_view)
{
    BOOST_TEST(format_sql(single_fmt, opts, makebv("hello\\")) == "SELECT 'hello\\\\';");
    BOOST_TEST(format_sql(single_fmt, opts, makebv("hello \xc3\xb1!")) == "SELECT 'hello \xc3\xb1!';");
    BOOST_TEST(format_sql(single_fmt, opts, blob_view()) == "SELECT '';");
}

BOOST_AUTO_TEST_CASE(individual_date)
{
    BOOST_TEST(format_sql(single_fmt, opts, date(2021, 1, 20)) == "SELECT '2021-01-20';");
    BOOST_TEST(format_sql(single_fmt, opts, date()) == "SELECT '0000-00-00';");
    BOOST_TEST(format_sql(single_fmt, opts, date(0xffff, 0xff, 0xff)) == "SELECT '65535-255-255';");
}

BOOST_AUTO_TEST_CASE(individual_datetime)
{
    BOOST_TEST(format_sql(single_fmt, opts, datetime(2021, 1, 20)) == "SELECT '2021-01-20 00:00:00.000000';");
    BOOST_TEST(
        format_sql(single_fmt, opts, datetime(1998, 1, 1, 21, 3, 5, 12)) ==
        "SELECT '1998-01-01 21:03:05.000012';"
    );
    BOOST_TEST(format_sql(single_fmt, opts, datetime()) == "SELECT '0000-00-00 00:00:00.000000';");
    BOOST_TEST(
        format_sql(single_fmt, opts, datetime(0xffff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xffffffff)) ==
        "SELECT '65535-255-255 255:255:255.4294967295';"
    );
}

BOOST_AUTO_TEST_CASE(individual_time)
{
    BOOST_TEST(format_sql(single_fmt, opts, maket(127, 1, 10, 123)) == "SELECT '127:01:10.000123';");
    BOOST_TEST(format_sql(single_fmt, opts, -maket(9, 1, 10)) == "SELECT '-09:01:10.000000';");
    BOOST_TEST(format_sql(single_fmt, opts, mysql_time()) == "SELECT '00:00:00.000000';");
    BOOST_TEST(format_sql(single_fmt, opts, (mysql_time::min)()) == "SELECT '-2562047788:00:54.775808';");
    BOOST_TEST(format_sql(single_fmt, opts, (mysql_time::max)()) == "SELECT '2562047788:00:54.775807';");
}

BOOST_AUTO_TEST_CASE(individual_field_view)
{
    field referenced("def\\");
    BOOST_TEST(format_sql(single_fmt, opts, field_view()) == "SELECT NULL;");
    BOOST_TEST(format_sql(single_fmt, opts, field_view(42)) == "SELECT 42;");
    BOOST_TEST(format_sql(single_fmt, opts, field_view("'abc'")) == "SELECT '\\'abc\\'';");
    BOOST_TEST(format_sql(single_fmt, opts, field_view(referenced)) == "SELECT 'def\\\\';");
}

BOOST_AUTO_TEST_CASE(individual_field)
{
    field f_lval("hol\"a");
    const field f_clval(42);
    BOOST_TEST(format_sql(single_fmt, opts, field()) == "SELECT NULL;");
    BOOST_TEST(format_sql(single_fmt, opts, field(4.2)) == "SELECT 4.2e+00;");
    BOOST_TEST(format_sql(single_fmt, opts, f_lval) == "SELECT 'hol\\\"a';");
    BOOST_TEST(format_sql(single_fmt, opts, f_clval) == "SELECT 42;");
}

BOOST_AUTO_TEST_CASE(individual_boost_optional)
{
    boost::optional<std::string> o_lval("abc");
    boost::optional<const std::string> co_lval("ab'c");
    const boost::optional<std::string> o_clval("\\");
    const boost::optional<const std::string> co_clval("abdef");
    BOOST_TEST(format_sql(single_fmt, opts, boost::optional<int>()) == "SELECT NULL;");
    BOOST_TEST(format_sql(single_fmt, opts, boost::optional<int>(42)) == "SELECT 42;");
    BOOST_TEST(format_sql(single_fmt, opts, o_lval) == "SELECT 'abc';");
    BOOST_TEST(format_sql(single_fmt, opts, co_lval) == "SELECT 'ab\\'c';");
    BOOST_TEST(format_sql(single_fmt, opts, o_clval) == "SELECT '\\\\';");
    BOOST_TEST(format_sql(single_fmt, opts, co_clval) == "SELECT 'abdef';");
}

#ifdef __cpp_lib_optional
BOOST_AUTO_TEST_CASE(individual_std_optional)
{
    std::optional<std::string> o_lval("abc");
    std::optional<const std::string> co_lval("ab'c");
    const std::optional<std::string> o_clval("\\");
    const std::optional<const std::string> co_clval("abdef");
    BOOST_TEST(format_sql(single_fmt, opts, std::optional<int>()) == "SELECT NULL;");
    BOOST_TEST(format_sql(single_fmt, opts, std::optional<int>(42)) == "SELECT 42;");
    BOOST_TEST(format_sql(single_fmt, opts, o_lval) == "SELECT 'abc';");
    BOOST_TEST(format_sql(single_fmt, opts, co_lval) == "SELECT 'ab\\'c';");
    BOOST_TEST(format_sql(single_fmt, opts, o_clval) == "SELECT '\\\\';");
    BOOST_TEST(format_sql(single_fmt, opts, co_clval) == "SELECT 'abdef';");
}
#endif

BOOST_AUTO_TEST_CASE(individual_identifier)
{
    constexpr const char* fmt = "SELECT {} FROM myt";
    BOOST_TEST(format_sql(fmt, opts, identifier("myfield")) == "SELECT `myfield` FROM myt");
    BOOST_TEST(format_sql(fmt, opts, identifier("myt", "myf")) == "SELECT `myt`.`myf` FROM myt");
    BOOST_TEST(
        format_sql(fmt, opts, identifier("mydb", "myt", "myf")) == "SELECT `mydb`.`myt`.`myf` FROM myt"
    );
    BOOST_TEST(format_sql(fmt, opts, identifier("inj`ect'ion")) == "SELECT `inj``ect'ion` FROM myt");
    BOOST_TEST(
        format_sql(fmt, opts, identifier("mo`e\\", "inj``ection", "att\nemmpts`")) ==
        "SELECT `mo``e\\`.`inj````ection`.`att\nemmpts``` FROM myt"
    );

    // Empty identifiers are not valid in MySQL but they shouldn't case formatting problems.
    // They are correctly rejected by MySQL (they don't cause problems)
    BOOST_TEST(format_sql(fmt, opts, identifier("")) == "SELECT `` FROM myt");
    BOOST_TEST(format_sql(fmt, opts, identifier("", "myf")) == "SELECT ``.`myf` FROM myt");
    BOOST_TEST(format_sql(fmt, opts, identifier("myt", "")) == "SELECT `myt`.`` FROM myt");
    BOOST_TEST(format_sql(fmt, opts, identifier("", "myt", "myf")) == "SELECT ``.`myt`.`myf` FROM myt");
    BOOST_TEST(format_sql(fmt, opts, identifier("mydb", "", "myf")) == "SELECT `mydb`.``.`myf` FROM myt");
    BOOST_TEST(format_sql(fmt, opts, identifier("mydb", "myt", "")) == "SELECT `mydb`.`myt`.`` FROM myt");
    BOOST_TEST(format_sql(fmt, opts, identifier("", "", "myf")) == "SELECT ``.``.`myf` FROM myt");
    BOOST_TEST(format_sql(fmt, opts, identifier("", "myt", "")) == "SELECT ``.`myt`.`` FROM myt");
    BOOST_TEST(format_sql(fmt, opts, identifier("mydb", "", "")) == "SELECT `mydb`.``.`` FROM myt");
    BOOST_TEST(format_sql(fmt, opts, identifier("", "", "")) == "SELECT ``.``.`` FROM myt");
}

BOOST_AUTO_TEST_CASE(individual_custom_type)
{
    auto actual = format_sql("SELECT * FROM myt WHERE {}", opts, custom::condition{"myfield", 42});
    string_view expected = "SELECT * FROM myt WHERE `myfield`=42";
    BOOST_TEST(actual == expected);
}

//
// Errors when formatting individual fields
//
template <class Arg>
error_code format_single_error(const Arg& arg)
{
    format_context ctx(opts);
    ctx.append_value(arg);
    return ctx.get().error();
}

BOOST_AUTO_TEST_CASE(individual_error)
{
    using float_limits = std::numeric_limits<float>;
    using double_limits = std::numeric_limits<double>;

    // float inf and nan
    BOOST_TEST(format_single_error(float_limits::infinity()) == client_errc::unformattable_value);
    BOOST_TEST(format_single_error(-float_limits::infinity()) == client_errc::unformattable_value);
    BOOST_TEST(format_single_error(float_limits::quiet_NaN()) == client_errc::unformattable_value);

    // double inf and nan
    BOOST_TEST(format_single_error(double_limits::infinity()) == client_errc::unformattable_value);
    BOOST_TEST(format_single_error(-double_limits::infinity()) == client_errc::unformattable_value);
    BOOST_TEST(format_single_error(double_limits::quiet_NaN()) == client_errc::unformattable_value);

    // strings and blobs with invalid characters
    BOOST_TEST(format_single_error("a\xc3'") == client_errc::invalid_encoding);
    BOOST_TEST(format_single_error(makebv("a\xff\xff")) == client_errc::invalid_encoding);

    // identifiers with invalid characters
    BOOST_TEST(format_single_error(identifier("a\xd8")) == client_errc::invalid_encoding);
    BOOST_TEST(format_single_error(identifier("a\xd8", "abc")) == client_errc::invalid_encoding);
    BOOST_TEST(format_single_error(identifier("a\xd8", "abc", "def")) == client_errc::invalid_encoding);
    BOOST_TEST(format_single_error(identifier("abc", "a\xc3 ")) == client_errc::invalid_encoding);
    BOOST_TEST(format_single_error(identifier("abc", "a\xc3 ", "def")) == client_errc::invalid_encoding);
    BOOST_TEST(format_single_error(identifier("abc", "def", "a\xd9")) == client_errc::invalid_encoding);
    BOOST_TEST(format_single_error(identifier("a\xc3", "\xff", "abc")) == client_errc::invalid_encoding);
    BOOST_TEST(format_single_error(identifier("a\xc3", "abc", "a\xdf")) == client_errc::invalid_encoding);
    BOOST_TEST(format_single_error(identifier("a\xc3", "\xff", "a\xd9")) == client_errc::invalid_encoding);
}

//
// Format strings: covers expanding a format string into an actual query
// using format_sql. This is specific to format_sql. Assumes that formatting
// individual arguments works
//

BOOST_AUTO_TEST_CASE(format_strings)
{
    // Empty string
    BOOST_TEST(format_sql("", opts) == "");

    // String without replacements
    BOOST_TEST(format_sql("SELECT 1", opts) == "SELECT 1");

    // Escaped curly braces
    BOOST_TEST(format_sql("SELECT '{{}}'", opts, 42) == "SELECT '{}'");
    BOOST_TEST(format_sql("SELECT '{{'", opts, 42) == "SELECT '{'");
    BOOST_TEST(format_sql("SELECT '}}'", opts, 42) == "SELECT '}'");
    BOOST_TEST(format_sql("SELECT '{{{{}}}}'", opts, 42) == "SELECT '{{}}'");
    BOOST_TEST(format_sql("SELECT '}}}}{{'", opts, 42) == "SELECT '}}{'");
    BOOST_TEST(format_sql("{{{}}}", opts, 42) == "{42}");
    BOOST_TEST(format_sql("SELECT '{{0}}'", opts, 42) == "SELECT '{0}'");
    BOOST_TEST(format_sql("SELECT '{{name}}'", opts, 42) == "SELECT '{name}'");

    // One format arg, possibly with text around it
    BOOST_TEST(format_sql("SELECT {}", opts, 42) == "SELECT 42");                // text{}
    BOOST_TEST(format_sql("{} OR 1=1", opts, 42) == "42 OR 1=1");                // {} text
    BOOST_TEST(format_sql("{}", opts, 42) == "42");                              // {}
    BOOST_TEST(format_sql("SELECT {} OR 1=1", opts, 42) == "SELECT 42 OR 1=1");  // text{}text

    // Two format args
    BOOST_TEST(format_sql("{}{}", opts, 42, "abc") == "42'abc'");        // {}{}
    BOOST_TEST(format_sql("{} + {}", opts, 42, "abc") == "42 + 'abc'");  // {}text{}
    BOOST_TEST(
        format_sql("WHERE a={} OR b={} OR 1=1", opts, 42, "abc") == "WHERE a=42 OR b='abc' OR 1=1"
    );  // text{}text{}text
    BOOST_TEST(format_sql("SELECT {} OR 1=1", opts, 42) == "SELECT 42 OR 1=1");

    // More format args
    BOOST_TEST(format_sql("IN({}, {}, {}, {})", opts, 1, 5, 2, "'abc'") == "IN(1, 5, 2, '\\'abc\\'')");

    // Explicit positional args
    BOOST_TEST(format_sql("SELECT {0}", opts, 42) == "SELECT 42");
    BOOST_TEST(format_sql("SELECT {1}, {0}", opts, 42, "abc") == "SELECT 'abc', 42");
    BOOST_TEST(format_sql("SELECT {0}, {0}", opts, 42) == "SELECT 42, 42");  // repeated

    // Named arguments
    BOOST_TEST(format_sql("SELECT {val}", opts, arg("val", 42)) == "SELECT 42");
    BOOST_TEST(
        format_sql("SELECT {val2}, {val}", opts, arg("val", 42), arg("val2", "abc")) == "SELECT 'abc', 42"
    );
    BOOST_TEST(format_sql("SELECT {Str1_geName}", opts, arg("Str1_geName", 42)) == "SELECT 42");
    BOOST_TEST(format_sql("SELECT {_name}", opts, arg("_name", 42)) == "SELECT 42");
    BOOST_TEST(format_sql("SELECT {name123}", opts, arg("name123", 42)) == "SELECT 42");
    BOOST_TEST(format_sql("SELECT {NAME}", opts, arg("NAME", 42)) == "SELECT 42");
    BOOST_TEST(format_sql("SELECT {a}", opts, arg("a", 42)) == "SELECT 42");

    // Named arguments can be referenced by position and automatically, too
    BOOST_TEST(format_sql("SELECT {}, {}", opts, arg("val", 42), arg("other", 50)) == "SELECT 42, 50");
    BOOST_TEST(format_sql("SELECT {1}, {0}", opts, arg("val", 42), arg("other", 50)) == "SELECT 50, 42");

    // Named arguments can be mixed with positional and automatic
    BOOST_TEST(format_sql("SELECT {}, {val}", opts, arg("val", 42)) == "SELECT 42, 42");
    BOOST_TEST(format_sql("SELECT {}, {val}", opts, 50, arg("val", 42)) == "SELECT 50, 42");
    BOOST_TEST(format_sql("SELECT {0}, {val}", opts, arg("val", 42)) == "SELECT 42, 42");

    // Unused arguments are ignored
    BOOST_TEST(format_sql("SELECT {}", opts, 42, "abc", nullptr) == "SELECT 42");
    BOOST_TEST(format_sql("SELECT {2}, {1}", opts, 42, "abc", nullptr, 4.2) == "SELECT NULL, 'abc'");
    BOOST_TEST(
        format_sql("SELECT {value}", opts, arg("a", 10), arg("value", 42), arg("a", "ac")) == "SELECT 42"
    );

    // Indices with leading zeroes are parsed correctly and not interpreted as octal
    BOOST_TEST(format_sql("SELECT {010}", opts, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12) == "SELECT 10");

    // spotcheck: {} characters in string values are not treated specially
    BOOST_TEST(format_sql("CONCAT({}, {})", opts, "{}", "a{b}c") == "CONCAT('{}', 'a{b}c')");
    BOOST_TEST(format_sql("CONCAT({}, {})", opts, "{", "a}c") == "CONCAT('{', 'a}c')");
    BOOST_TEST(format_sql("CONCAT({}, {})", opts, "{{}}", "{{1}}") == "CONCAT('{{}}', '{{1}}')");
    BOOST_TEST(format_sql("CONCAT({}, {})", opts, "'\\{", "\"}") == "CONCAT('\\'\\\\{', '\\\"}')");

    // Format strings with non-ascii (but valid) characters
    BOOST_TEST(format_sql("SELECT `e\xc3\xb1u` + {};", opts, 42) == "SELECT `e\xc3\xb1u` + 42;");
    BOOST_TEST(format_sql("\xc3\xb1{}", opts, "abc") == "\xc3\xb1'abc'");
}

// Custom charset function
static std::size_t ff_charset_next_char(string_view s) noexcept
{
    auto c = static_cast<unsigned char>(s[0]);
    if (c == 0xff)  // 0xff marks a two-byte character
        return s.size() > 1u ? 2u : 0u;
    return 1u;
};
constexpr character_set ff_charset{"ff_charset", ff_charset_next_char};

// backslash_slashes and character set are propagated
BOOST_AUTO_TEST_CASE(format_strings_options_propagated)
{
    format_options opts_charset{ff_charset, true};
    format_options opts_backslashes{ff_charset, false};

    // Charset affects format strings
    BOOST_TEST(format_sql("SELECT \xffh + {};", opts_charset, 42) == "SELECT \xffh + 42;");

    // Charset affects string values and identifiers
    BOOST_TEST(format_sql("SELECT {};", opts_charset, "ab\xff''") == "SELECT 'ab\xff'\\'';");
    BOOST_TEST(format_sql("SELECT {};", opts_charset, identifier("ab\xff``")) == "SELECT `ab\xff````;");

    // Backslash escapes affects how string values are escaped
    BOOST_TEST(format_sql("SELECT {};", opts_backslashes, "ab'cd") == "SELECT 'ab''cd';");
    BOOST_TEST(format_sql("SELECT {};", opts_backslashes, "ab\"cd") == "SELECT 'ab\"cd';");
}

// In a character set with ASCII-compatible continuation characters, we correctly
// interpret {} characters as continuations, rather than trying to expand them
BOOST_AUTO_TEST_CASE(format_strings_brace_continuation)
{
    format_options custom_opts{ff_charset, true};

    BOOST_TEST(format_sql("SELECT \xff{ + {};", custom_opts, 42) == "SELECT \xff{ + 42;");
    BOOST_TEST(format_sql("SELECT \xff} + {};", custom_opts, 42) == "SELECT \xff} + 42;");
    BOOST_TEST(format_sql("SELECT \xff{}} + {};", custom_opts, 42) == "SELECT \xff{} + 42;");
}

BOOST_AUTO_TEST_CASE(format_strings_invalid)
{
    struct
    {
        string_view name;
        string_view format_str;
        string_view expected_diag;
    } test_cases[] = {
  // Simply invalid
        {"unbalanced_{",             "SELECT { bad",        "invalid format string"                                                            },
        {"unbalanced_{_eof",         "SELECT {",            "invalid format string"                                                            },
        {"unbalanced_}",             "SELECT } bad",        "unbalanced '}' in format string"                                                  },
        {"unbalanced_}_after_field", "SELECT {}} bad",      "unbalanced '}' in format string"                                                  },
        {"unbalanced_}_eof",         "SELECT }",            "unbalanced '}' in format string"                                                  },
        {"invalid_character",
         "SELECT \xc3 bad",                                 "the format string contains characters that are invalid in the given character set"},

 // Named argument problems
        {"name_starts_number",       "SELECT {0name}",      "invalid format string"                                                            },
        {"name_starts_invalid",      "SELECT {!name}",      "invalid format string"                                                            },
        {"name_ends_invalid",        "SELECT {name!}",      "invalid format string"                                                            },
        {"name_contains_invalid",    "SELECT {na'me}",      "invalid format string"                                                            },
        {"name_spaces",              "SELECT { name }",     "invalid format string"                                                            },
        {"name_non_ascii",           "SELECT {e\xc3\xb1p}", "invalid format string"                                                            },
        {"name_format_spec",         "SELECT {name:abc}",   "invalid format string"                                                            },
        {"name_format_spec_empty",   "SELECT {name:}",      "invalid format string"                                                            },
        {"name_eof",                 "SELECT {name",        "invalid format string"                                                            },
        {"name_not_found",           "SELECT {name} {bad}", "named argument not found"                                                         },

 // Explicit indexing problems
        {"index_hex",                "SELECT {0x10}",       "invalid format string"                                                            },
        {"index_hex_noprefix",       "SELECT {1a}",         "invalid format string"                                                            },
        {"index_spaces",             "SELECT { 1 }",        "invalid format string"                                                            },
        {"index_format_spec",        "SELECT {0:abc}",      "invalid format string"                                                            },
        {"index_format_spec_empty",  "SELECT {0:}",         "invalid format string"                                                            },
        {"index_eof",                "SELECT {0",           "invalid format string"                                                            },
        {"index_gt_max",             "SELECT {65536}",      "invalid format string"                                                            },
        {"index_negative",           "SELECT {-1}",         "invalid format string"                                                            },
        {"index_float",              "SELECT {4.2}",        "invalid format string"                                                            },
        {"index_not_found",          "SELECT {2}",          "argument index out of range"                                                      },
        {"index_to_manual",          "SELECT {0}, {}",      "cannot switch from explicit to automatic indexing"                                },

 // Auto indexing problems
        {"auto_format_spec",         "SELECT {:abc}",       "invalid format string"                                                            },
        {"auto_format_spec_empty",   "SELECT {:}",          "invalid format string"                                                            },
        {"auto_replacement_inside",  "SELECT { {} }",       "invalid format string"                                                            },
        {"auto_too_many_args",       "SELECT {}, {}, {}",   "argument index out of range"                                                      },
        {"auto_to_manual",           "SELECT {}, {0}",      "cannot switch from automatic to explicit indexing"                                },
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            BOOST_CHECK_EXCEPTION(
                format_sql(runtime(tc.format_str), opts, 42, arg("name", "abc")),
                error_with_diagnostics,
                [&](const error_with_diagnostics& err) {
                    std::string expected_diag = "Formatting SQL: ";
                    expected_diag += tc.expected_diag;
                    BOOST_TEST(err.code() == client_errc::invalid_format_string);
                    BOOST_TEST(err.get_diagnostics().client_message() == expected_diag);
                    return true;
                }
            );
        }
    }
}

BOOST_AUTO_TEST_CASE(format_strings_invalid_arguments)
{
    // When passed invalid arguments (like strings with invalid UTF-8 or NaNs) we throw
    BOOST_CHECK_EXCEPTION(
        format_sql("SELECT {}", opts, "Invalid\xffUTF8"),
        boost::system::system_error,
        [&](const boost::system::system_error& err) {
            std::string expected_diag =
                "Formatting SQL: An invalid byte sequence was found while trying to decode a string. "
                "[mysql.client:17]";
            BOOST_TEST(err.code() == client_errc::invalid_encoding);
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
    auto get = [](format_context_base& ctx) { return static_cast<format_context&>(ctx).get().value(); };

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
    BOOST_TEST(ctx.get().value() == "SELECT '\xff{abc' + 'abd\xff{}' + `i``d``ent\xff`ifier`");
}

BOOST_AUTO_TEST_CASE(format_context_backslashes)
{
    format_options opts_charset{ff_charset, false};

    format_context ctx(opts_charset);
    ctx.append_raw("SELECT ")
        .append_value("ab'cd\"ef")
        .append_raw(" + ")
        .append_value(identifier("identif`ier"));
    BOOST_TEST(ctx.get().value() == "SELECT 'ab''cd\"ef' + `identif``ier`");
}

BOOST_AUTO_TEST_CASE(format_context_error)
{
    // Helper
    auto get = [](format_context_base& ctx) { return static_cast<format_context&>(ctx).get().error(); };

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

/**
 * unit tests
 *   construct from another string: uses that and doesn't clear
 *   move construct: takes ownserhip of the other string
 *   move construct: propagates options
 *   move construct: propagates error state
 *   move assign: takes ownserhip of the other string
 *   move assign: propagates options
 *   move assign: propagates error state
 *   can be used with vector<char>, string with custom allocator, static_string?
 *   archetype
 *   archetype + default constructible
 *   archetype + move assignable
 * format_context
 *    the string is not cleared when created
 *    can be created from strings that are not std::string (TODO: we should test this in other places, too)
 */

BOOST_AUTO_TEST_SUITE_END()
