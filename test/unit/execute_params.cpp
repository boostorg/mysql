//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/execute_params.hpp>
#include <boost/mysql/field.hpp>
#include <boost/mysql/field_view.hpp>

#include <boost/test/unit_test.hpp>

#include <forward_list>
#include <type_traits>

using boost::mysql::field;
using boost::mysql::field_view;
using boost::mysql::make_execute_params;

// Make Boost.Test ignore list iterators when printing
BOOST_TEST_DONT_PRINT_LOG_VALUE(std::forward_list<field_view>::iterator)
BOOST_TEST_DONT_PRINT_LOG_VALUE(std::forward_list<field_view>::const_iterator)
BOOST_TEST_DONT_PRINT_LOG_VALUE(std::forward_list<field>::iterator)
BOOST_TEST_DONT_PRINT_LOG_VALUE(std::forward_list<field>::const_iterator)

BOOST_AUTO_TEST_SUITE(test_execute_params)

// collection
BOOST_AUTO_TEST_SUITE(test_make_execute_params_collection)

BOOST_AUTO_TEST_CASE(c_array_field_view)
{
    field_view arr[10];
    auto params = make_execute_params(arr);
    BOOST_TEST(params.first() == std::begin(arr));
    BOOST_TEST(params.last() == std::end(arr));
    static_assert(std::is_same<decltype(params.first()), const field_view*>::value, "");
}

BOOST_AUTO_TEST_CASE(forward_list_field)
{
    std::forward_list<field> l{field_view("a"), field_view("b")};
    auto params = make_execute_params(l);
    BOOST_TEST(params.first() == std::begin(l));
    BOOST_TEST(params.last() == std::end(l));
    static_assert(
        std::is_same<decltype(params.first()), std::forward_list<field>::const_iterator>::value,
        ""
    );
}

BOOST_AUTO_TEST_SUITE_END()

// iterators
BOOST_AUTO_TEST_SUITE(test_make_execute_params_range)

BOOST_AUTO_TEST_CASE(c_array)
{
    field_view arr[10];
    auto params = make_execute_params(&arr[0], &arr[2]);
    BOOST_TEST(params.first() == &arr[0]);
    BOOST_TEST(params.last() == &arr[2]);
    static_assert(std::is_same<decltype(params.first()), field_view*>::value, "");
}

BOOST_AUTO_TEST_CASE(forward_list)
{
    std::forward_list<field_view> l{field_view("a"), field_view("b")};
    auto params = make_execute_params(l.begin(), std::next(l.begin()));
    BOOST_TEST(params.first() == l.begin());
    BOOST_TEST(params.last() == std::next(l.begin()));
    static_assert(
        std::is_same<decltype(params.first()), std::forward_list<field_view>::iterator>::value,
        ""
    );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()