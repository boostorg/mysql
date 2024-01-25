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
#include <boost/test/unit_test.hpp>

#include <string>
#include <vector>

#include "test_common/create_basic.hpp"
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
using custom_string = std::basic_string<char, std::char_traits<char>, custom_allocator<char>>;
using custom_blob = std::vector<unsigned char, custom_allocator<unsigned char>>;

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

/**
 * injection attempts
 *    error thrown on invalid encoding
 * charset
 *    multi-byte characters allowed in format strings
 *    invalid multi-byte characters in format strings throw
 *    multi-byte characters with { continuation cause no problems
 *    multi-byte characters with } continuation cause no problems
 * the string is cleared on input
 * backslash_escapes is correctly propagated to escaping
 * format_context
 *    the string is not cleared when created
 *    can be created from strings that are not std::string (TODO: we should test this in other places, too)
 *    add_raw just appends, even if it has special chars
 *    add_value with basic types
 *    add_value with writable fields like optionals
 *    add_value with custom formatters
 *    neither add_raw not add_value clear the string
 * float, double: isnan, isinf
 */

BOOST_AUTO_TEST_SUITE(test_format_sql)

constexpr format_options opts{utf8mb4_charset, true};
constexpr auto single_fmt = "SELECT {};";

//
// Verify that formatting individual values work. This is tested using format_sql
// because it's convenient, but also covers basic_format_context
//

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
    BOOST_TEST(format_sql(single_fmt, opts, (int)-2147483648) == "SELECT -2147483648;");
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
    BOOST_TEST(format_sql(single_fmt, opts, custom_string("abc'")) == "SELECT 'abc\\'';");
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
    BOOST_TEST(format_sql(single_fmt, opts, custom_blob{0x00, 0x01, 0x02}) == "SELECT '\\0\1\2';");
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

BOOST_AUTO_TEST_CASE(individual_raw_sql)
{
    BOOST_TEST(format_sql(single_fmt, opts, raw_sql("")) == "SELECT ;");
    BOOST_TEST(format_sql(single_fmt, opts, raw_sql("42")) == "SELECT 42;");
    BOOST_TEST(format_sql(single_fmt, opts, raw_sql("'abc' OR 1=1")) == "SELECT 'abc' OR 1=1;");
}

BOOST_AUTO_TEST_CASE(individual_identifier)
{
    string_view fmt = "SELECT {} FROM myt";
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
}

BOOST_AUTO_TEST_CASE(individual_custom_type)
{
    auto actual = format_sql("SELECT * FROM myt WHERE {}", opts, custom::condition("myfield", 42));
    string_view expected = "SELECT * FROM myt WHERE `myfield`=42";
    BOOST_TEST(actual == expected);
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
                format_sql(tc.format_str, opts, 42, arg("name", "abc")),
                error_with_diagnostics,
                [&](const error_with_diagnostics& err) {
                    std::string expected_diag = "Formatting SQL: ";
                    expected_diag += tc.expected_diag;
                    BOOST_TEST(err.code() == error_code(client_errc::invalid_format_string));
                    BOOST_TEST(err.get_diagnostics().client_message() == expected_diag);
                    return true;
                }
            );
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
