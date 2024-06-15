

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

#include <cstddef>
#include <initializer_list>

#include "format_common.hpp"
#include "test_common/printing.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;

namespace boost {
namespace mysql {
namespace test {

// When formatted, outputs the spec to the output context
struct echo_spec
{
};

// Parse returns begin + N
template <std::size_t N>
struct parse_consume
{
};

}  // namespace test

template <>
struct formatter<echo_spec>
{
    string_view spec;

    const char* parse(const char* it, const char* end)
    {
        spec = {it, end};
        return end;
    }

    void format(echo_spec, format_context_base& ctx) const { ctx.append_raw(runtime(spec)); }
};

template <std::size_t N>
struct formatter<parse_consume<N>>
{
    const char* parse(const char* it, const char*) { return it + N; }
    void format(parse_consume<N>, format_context_base&) const {}
};

}  // namespace mysql
}  // namespace boost

BOOST_AUTO_TEST_SUITE(test_format_sql_custom)

constexpr format_options opts{utf8mb4_charset, true};

// format processing
//    you can access the parsed formatters
//    you can access the character set and other options
//    you can call set_error
// spotcheck over custom::condition

// We pass the correct format spec to parse
BOOST_AUTO_TEST_CASE(parse_passed_format_specs)
{
    std::initializer_list<format_arg> named_args{
        {"name", echo_spec{}}
    };

    // No spec
    BOOST_TEST(format_sql(opts, "{}", echo_spec{}) == "");
    BOOST_TEST(format_sql(opts, "{:}", echo_spec{}) == "");
    BOOST_TEST(format_sql(opts, "{0}", echo_spec{}) == "");
    BOOST_TEST(format_sql(opts, "{0:}", echo_spec{}) == "");
    BOOST_TEST(format_sql(opts, "{name}", named_args) == "");
    BOOST_TEST(format_sql(opts, "{name:}", named_args) == "");

    // Single char
    BOOST_TEST(format_sql(opts, "{:k}", echo_spec{}) == "k");
    BOOST_TEST(format_sql(opts, "{0:u}", echo_spec{}) == "u");
    BOOST_TEST(format_sql(opts, "{name:p}", named_args) == "p");

    // Multiple chars
    BOOST_TEST(format_sql(opts, "{:some}", echo_spec{}) == "some");
    BOOST_TEST(format_sql(opts, "{0:chars}", echo_spec{}) == "chars");
    BOOST_TEST(format_sql(opts, "{name:here}", named_args) == "here");

    // All ASCII characters allowed, except for {}
    BOOST_TEST(
        format_sql(opts, "{:abcdefghijklmnopqrstuvwxyz}", echo_spec{}) == "abcdefghijklmnopqrstuvwxyz"
    );
    BOOST_TEST(
        format_sql(opts, "{0:ABCDEFGHIJKLMNOPQRSTUVWXYZ}", echo_spec{}) == "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    );
    BOOST_TEST(format_sql(opts, "{name:0abAZ12 3456789}", named_args) == "0abAZ12 3456789");
    BOOST_TEST(
        format_sql(opts, "{:!\"#$%&'()*+,-./:;<=>?@[]\\^_`|~}", echo_spec{}) ==
        "!\"#$%&'()*+,-./:;<=>?@[]\\^_`|~"
    );
}

// Returning something != end in parse is an error
BOOST_AUTO_TEST_CASE(parse_error)
{
    BOOST_TEST(
        format_single_error("{:abc}", parse_consume<0>()) == client_errc::format_string_invalid_specifier
    );
    BOOST_TEST(
        format_single_error("{:abc}", parse_consume<1>()) == client_errc::format_string_invalid_specifier
    );
    BOOST_TEST(
        format_single_error("{:abc}", parse_consume<2>()) == client_errc::format_string_invalid_specifier
    );
    BOOST_TEST(format_single_error("{:abc}", parse_consume<3>()) == error_code());
}

BOOST_AUTO_TEST_SUITE_END()