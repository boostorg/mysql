//
// Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "boost/mysql/execute_params.hpp"
#include "boost/mysql/value.hpp"
#include "test_common.hpp"
#include "assert_type_equals.hpp"
#include <boost/test/unit_test.hpp>
#include <forward_list>
#include <type_traits>

using boost::mysql::make_execute_params;
using boost::mysql::value;
using boost::mysql::test::assert_type_equals;

// Make execute_args printable by Boost.Test
namespace boost {
namespace mysql {

template <typename ValueForwardIterator>
std::ostream& boost_test_print_type(std::ostream& os, const execute_params<ValueForwardIterator>& v)
{
    return os 
        << "execute_params<"
        << boost::typeindex::type_id<ValueForwardIterator>().pretty_name()
        << ">("
        << v.first()
        << ", "
        << v.last()
        << ")";
}

} // mysql
} // boost

// Make Boost.Test ignore list iterators when printing
BOOST_TEST_DONT_PRINT_LOG_VALUE(std::forward_list<value>::iterator)
BOOST_TEST_DONT_PRINT_LOG_VALUE(std::forward_list<value>::const_iterator)

BOOST_AUTO_TEST_SUITE(test_execute_params)

// setters
BOOST_AUTO_TEST_SUITE(test_setters)

BOOST_AUTO_TEST_CASE(set_first)
{
    value arr [10];
    auto params = make_execute_params(arr);
    params.set_first(&arr[1]);
    BOOST_TEST(params.first() == &arr[1]);
    BOOST_TEST(params.last() == std::end(arr));
}

BOOST_AUTO_TEST_CASE(set_last)
{
    value arr [10];
    auto params = make_execute_params(arr);
    params.set_last(&arr[1]);
    BOOST_TEST(params.first() == std::begin(arr));
    BOOST_TEST(params.last() == &arr[1]);
}

BOOST_AUTO_TEST_SUITE_END()

// collection
BOOST_AUTO_TEST_SUITE(test_make_execute_params_collection)

BOOST_AUTO_TEST_CASE(c_array)
{
    value arr [10];
    auto params = make_execute_params(arr);
    BOOST_TEST(params.first() == std::begin(arr));
    BOOST_TEST(params.last() == std::end(arr));
    assert_type_equals<decltype(params.first()), const value*>();
}

BOOST_AUTO_TEST_CASE(forward_list)
{
    std::forward_list<value> l { value("a"), value("b") };
    auto params = make_execute_params(l);
    BOOST_TEST(params.first() == std::begin(l));
    BOOST_TEST(params.last() == std::end(l));
    assert_type_equals<decltype(params.first()), std::forward_list<value>::const_iterator>();
}

BOOST_AUTO_TEST_SUITE_END()

// iterators
BOOST_AUTO_TEST_SUITE(test_make_execute_params_range)

BOOST_AUTO_TEST_CASE(c_array)
{
    value arr [10];
    auto params = make_execute_params(&arr[0], &arr[2]);
    BOOST_TEST(params.first() == &arr[0]);
    BOOST_TEST(params.last() == &arr[2]);
    assert_type_equals<decltype(params.first()), value*>();
}

BOOST_AUTO_TEST_CASE(forward_list)
{
    std::forward_list<value> l { value("a"), value("b") };
    auto params = make_execute_params(l.begin(), std::next(l.begin()));
    BOOST_TEST(params.first() == l.begin());
    BOOST_TEST(params.last() == std::next(l.begin()));
    assert_type_equals<decltype(params.first()), std::forward_list<value>::iterator>();
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()