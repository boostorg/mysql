//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/row.hpp>
#include <boost/mysql/connection.hpp>
#include <boost/mysql/detail/auxiliar/bytestring.hpp>
#include <boost/mysql/field_view.hpp>
#include "test_common.hpp"
#include <boost/test/unit_test_suite.hpp>
#include <boost/utility/string_view_fwd.hpp>
#include <vector>

using namespace boost::mysql::test;
using boost::mysql::row;
using boost::mysql::field_view;
using boost::mysql::detail::bytestring;

BOOST_AUTO_TEST_SUITE(test_row)

// Constructors
BOOST_AUTO_TEST_SUITE(constructors)

static std::vector<field_view> string_value_from_buffer(const bytestring& buffer)
{
    return make_value_vector(boost::string_view(reinterpret_cast<const char*>(buffer.data()), buffer.size()));
}

BOOST_AUTO_TEST_CASE(init_ctor)
{
    // Given a value vector with strings pointing into a buffer, after initializing
    // the row via moves, the string still points to valid memory
    bytestring buffer {'a', 'b', 'c', 'd'};
    auto values = string_value_from_buffer(buffer);
    row r (std::move(values), std::move(buffer));
    buffer = {'e', 'f'};
    BOOST_TEST(r.values()[0] == field_view("abcd"));
}

BOOST_AUTO_TEST_CASE(move_ctor)
{
    // Given a row with strings, after a move construction,
    // the string still points to valid memory
    bytestring buffer {'a', 'b', 'c', 'd'};
    auto values = string_value_from_buffer(buffer);
    row r (std::move(values), std::move(buffer));
    row r2 (std::move(r));
    r = row({}, {'e', 'f'});
    BOOST_TEST(r2.values()[0] == field_view("abcd"));
}

BOOST_AUTO_TEST_CASE(move_assignment)
{
    // Given a row with strings, after a move assignment,
    // the string still points to valid memory
    bytestring buffer {'a', 'b', 'c', 'd'};
    auto values = string_value_from_buffer(buffer);
    row r (std::move(values), std::move(buffer));
    row r2;
    r2 = std::move(r);
    r = row({}, {'e', 'f'});
    BOOST_TEST(r2.values()[0] == field_view("abcd"));
}

BOOST_AUTO_TEST_SUITE_END()

// Clear
BOOST_AUTO_TEST_SUITE(clear)

BOOST_AUTO_TEST_CASE(empty_row)
{
    row r;
    r.clear();
    BOOST_TEST(r.values().empty());
}

BOOST_AUTO_TEST_CASE(non_empty_row)
{
    row r = makerow("abc");
    r.clear();
    BOOST_TEST(r.values().empty());
}

BOOST_AUTO_TEST_SUITE_END()


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

