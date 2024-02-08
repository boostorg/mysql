//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/character_set.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>
#include <boost/mysql/format_sql.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/system/system_error.hpp>
#include <boost/test/unit_test.hpp>

#include "format_common.hpp"
#include "test_common/printing.hpp"

using namespace boost::mysql;

//
// Format strings: covers expanding a format string into an actual query
// using format_sql. This is specific to format_sql. Assumes that formatting
// individual arguments works
//
BOOST_AUTO_TEST_SUITE(test_format_strings)

constexpr format_options opts{utf8mb4_charset, true};

BOOST_AUTO_TEST_CASE(success)
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

// backslash_slashes and character set are propagated
BOOST_AUTO_TEST_CASE(options_propagated)
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

BOOST_AUTO_TEST_CASE(error)
{
    struct
    {
        string_view name;
        string_view format_str;
        error_code expected_ec;
    } test_cases[] = {
  // Simply invalid
        {"unbalanced_{",             "SELECT { bad",        client_errc::format_string_invalid_syntax  },
        {"unbalanced_{_eof",         "SELECT {",            client_errc::format_string_invalid_syntax  },
        {"unbalanced_}",             "SELECT } bad",        client_errc::format_string_invalid_syntax  },
        {"unbalanced_}_after_field", "SELECT {}} bad",      client_errc::format_string_invalid_syntax  },
        {"unbalanced_}_eof",         "SELECT }",            client_errc::format_string_invalid_syntax  },
        {"invalid_character",        "SELECT \xc3 bad",     client_errc::format_string_invalid_encoding},

 // Named argument problems
        {"name_starts_number",       "SELECT {0name}",      client_errc::format_string_invalid_syntax  },
        {"name_starts_invalid",      "SELECT {!name}",      client_errc::format_string_invalid_syntax  },
        {"name_ends_invalid",        "SELECT {name!}",      client_errc::format_string_invalid_syntax  },
        {"name_contains_invalid",    "SELECT {na'me}",      client_errc::format_string_invalid_syntax  },
        {"name_spaces",              "SELECT { name }",     client_errc::format_string_invalid_syntax  },
        {"name_non_ascii",           "SELECT {e\xc3\xb1p}", client_errc::format_string_invalid_syntax  },
        {"name_format_spec",         "SELECT {name:abc}",   client_errc::format_string_invalid_syntax  },
        {"name_format_spec_empty",   "SELECT {name:}",      client_errc::format_string_invalid_syntax  },
        {"name_eof",                 "SELECT {name",        client_errc::format_string_invalid_syntax  },
        {"name_not_found",           "SELECT {name} {bad}", client_errc::format_arg_not_found          },

 // Explicit indexing problems
        {"index_hex",                "SELECT {0x10}",       client_errc::format_string_invalid_syntax  },
        {"index_hex_noprefix",       "SELECT {1a}",         client_errc::format_string_invalid_syntax  },
        {"index_spaces",             "SELECT { 1 }",        client_errc::format_string_invalid_syntax  },
        {"index_format_spec",        "SELECT {0:abc}",      client_errc::format_string_invalid_syntax  },
        {"index_format_spec_empty",  "SELECT {0:}",         client_errc::format_string_invalid_syntax  },
        {"index_eof",                "SELECT {0",           client_errc::format_string_invalid_syntax  },
        {"index_gt_max",             "SELECT {65536}",      client_errc::format_string_invalid_syntax  },
        {"index_negative",           "SELECT {-1}",         client_errc::format_string_invalid_syntax  },
        {"index_float",              "SELECT {4.2}",        client_errc::format_string_invalid_syntax  },
        {"index_not_found",          "SELECT {2}",          client_errc::format_arg_not_found          },
        {"index_to_manual",          "SELECT {0}, {}",      client_errc::format_string_manual_auto_mix },

 // Auto indexing problems
        {"auto_format_spec",         "SELECT {:abc}",       client_errc::format_string_invalid_syntax  },
        {"auto_format_spec_empty",   "SELECT {:}",          client_errc::format_string_invalid_syntax  },
        {"auto_replacement_inside",  "SELECT { {} }",       client_errc::format_string_invalid_syntax  },
        {"auto_too_many_args",       "SELECT {}, {}, {}",   client_errc::format_arg_not_found          },
        {"auto_to_manual",           "SELECT {}, {0}",      client_errc::format_string_manual_auto_mix },
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            format_context ctx(opts);
            format_sql_to(runtime(tc.format_str), ctx, 42, arg("name", "abc"));
            BOOST_TEST(std::move(ctx).get().error() == tc.expected_ec);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
