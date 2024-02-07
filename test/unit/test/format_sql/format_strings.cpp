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

#include <string>

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

BOOST_AUTO_TEST_CASE(invalid_arguments)
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

BOOST_AUTO_TEST_SUITE_END()
