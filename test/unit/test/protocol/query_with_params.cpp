//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/character_set.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/format_sql.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/impl/internal/protocol/query_with_params.hpp>
#include <boost/mysql/impl/internal/protocol/serialization.hpp>

#include <boost/test/unit_test.hpp>

#include <cstdint>
#include <vector>

#include "serialization_test.hpp"
#include "test_common/printing.hpp"
#include "test_unit/create_query_frame.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;

BOOST_AUTO_TEST_SUITE(test_query_with_params)

BOOST_AUTO_TEST_CASE(success)
{
    const std::array<format_arg, 2> args{
        {{"", "abc"}, {"", 42}}
    };
    detail::query_with_params input{
        constant_string_view("SELECT {}, {}"),
        args,
        {utf8mb4_charset, true}
    };
    auto expected = create_query_body("SELECT 'abc', 42");
    do_serialize_test(input, expected);
}

BOOST_AUTO_TEST_CASE(success_no_params)
{
    detail::query_with_params input{
        constant_string_view("SELECT 42"),
        {},
        {utf8mb4_charset, true}
    };
    auto expected = create_query_body("SELECT 42");
    do_serialize_test(input, expected);
}

BOOST_AUTO_TEST_CASE(error)
{
    std::vector<std::uint8_t> buff;
    detail::query_with_params input{
        "SELECT {}",
        {},
        {utf8mb4_charset, true}
    };
    auto res = detail::serialize_top_level(input, buff);
    BOOST_TEST(res.err == client_errc::format_arg_not_found);
}

BOOST_AUTO_TEST_SUITE_END()
