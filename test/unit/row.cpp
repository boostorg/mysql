//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "boost/mysql/row.hpp"
#include "test_common.hpp"

using namespace boost::mysql::test;
using boost::mysql::row;

BOOST_AUTO_TEST_SUITE(test_row)

// Equality operators
BOOST_AUTO_TEST_SUITE(operators_eq_ne)

BOOST_AUTO_TEST_CASE(both_empty)
{
    BOOST_TEST(row() == row());
    BOOST_TEST(!(row() != row()));
}

BOOST_AUTO_TEST_CASE(one_empty_other_not_empty)
{
    row empty_row;
    row non_empty_row = makerow("a_value");
    BOOST_TEST(!(empty_row == non_empty_row));
    BOOST_TEST(empty_row != non_empty_row);
}

BOOST_AUTO_TEST_CASE(subset)
{
    row lhs = makerow("a_value", 42);
    row rhs = makerow("a_value");
    BOOST_TEST(!(lhs == rhs));
    BOOST_TEST(lhs != rhs);
}

BOOST_AUTO_TEST_CASE(same_size_different_values)
{
    row lhs = makerow("a_value", 42);
    row rhs = makerow("another_value", 42);
    BOOST_TEST(!(lhs == rhs));
    BOOST_TEST(lhs != rhs);
}

BOOST_AUTO_TEST_CASE(same_size_and_values)
{
    row lhs = makerow("a_value", 42);
    row rhs = makerow("a_value", 42);
    BOOST_TEST(lhs == rhs);
    BOOST_TEST(!(lhs != rhs));
}

BOOST_AUTO_TEST_SUITE_END() // operators_eq_ne

// Stream operators
BOOST_AUTO_TEST_SUITE(operator_stream)

BOOST_AUTO_TEST_CASE(empty_row)
{
    BOOST_TEST(stringize(row()) == "{}");
}

BOOST_AUTO_TEST_CASE(one_element)
{
    BOOST_TEST(stringize(makerow(42)) == "{42}");
}

BOOST_AUTO_TEST_CASE(two_elements)
{
    auto actual = stringize(makerow("value", nullptr));
    BOOST_TEST(actual == "{value, <NULL>}");
}

BOOST_AUTO_TEST_CASE(three_elements)
{
    auto actual = stringize(makerow("value", std::uint32_t(2019), 3.14f));
    BOOST_TEST(actual == "{value, 2019, 3.14}");
}

BOOST_AUTO_TEST_SUITE_END() // operator_stream

BOOST_AUTO_TEST_SUITE_END() // test_row

