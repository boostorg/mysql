//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/error.hpp>
#include "test_common.hpp"

using boost::mysql::errc;
using boost::mysql::detail::error_to_string;
using boost::mysql::error_info;
using boost::mysql::test::stringize;

BOOST_AUTO_TEST_SUITE(test_error)

// error_to_string
BOOST_AUTO_TEST_SUITE(errc_error_to_string)

BOOST_AUTO_TEST_CASE(ok)
{
    BOOST_TEST(error_to_string(errc::ok) == "No error");
}

BOOST_AUTO_TEST_CASE(client_error)
{
    BOOST_TEST(error_to_string(errc::sequence_number_mismatch)
        == "Mismatched sequence numbers");
}

BOOST_AUTO_TEST_CASE(server_error)
{
    BOOST_TEST(error_to_string(errc::bad_db_error) == "bad_db_error");
}

BOOST_AUTO_TEST_CASE(unknown_error_out_of_range)
{
    BOOST_TEST(error_to_string(static_cast<errc>(0xfffefdfc)) == "<unknown error>");
}

BOOST_AUTO_TEST_CASE(unknown_error_server_range)
{
    BOOST_TEST(error_to_string(static_cast<errc>(1009)) == "<unknown error>");
}

BOOST_AUTO_TEST_CASE(unknown_error_between_server_and_client_range)
{
    BOOST_TEST(error_to_string(static_cast<errc>(5000)) == "<unknown error>");
}

BOOST_AUTO_TEST_SUITE_END() // errc_error_to_string

BOOST_AUTO_TEST_CASE(errc_operator_stream)
{
    BOOST_TEST(stringize(errc::ok) == "No error");
}

// errro_info
BOOST_AUTO_TEST_SUITE(test_error_info)

BOOST_AUTO_TEST_CASE(operator_equals)
{
    BOOST_TEST(error_info() == error_info());
    BOOST_TEST(error_info("abc") == error_info("abc"));
    BOOST_TEST(!(error_info() == error_info("abc")));
    BOOST_TEST(!(error_info("def") == error_info("abc")));
}

BOOST_AUTO_TEST_CASE(operator_not_equals)
{
    BOOST_TEST(!(error_info() != error_info()));
    BOOST_TEST(!(error_info("abc") != error_info("abc")));
    BOOST_TEST(error_info() != error_info("abc"));
    BOOST_TEST(error_info("def") != error_info("abc"));
}

BOOST_AUTO_TEST_CASE(operator_stream)
{
    BOOST_TEST(stringize(error_info("abc")) == "abc");
}

BOOST_AUTO_TEST_SUITE_END() // test_error_info

// error_code construction from errc
BOOST_AUTO_TEST_CASE(error_code_from_errc)
{
    boost::mysql::error_code code (errc::protocol_value_error);
    BOOST_TEST(code.value() == static_cast<int>(errc::protocol_value_error));
}

BOOST_AUTO_TEST_SUITE_END() // test_error
