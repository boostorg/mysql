//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_suite.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/type_index.hpp>
#include <boost/type_index/stl_type_index.hpp>
#include <boost/mysql/detail/auxiliar/value_type_traits.hpp>
#include <cstddef>
#include <functional>
#include <iostream>
#include <string>
#include <type_traits>
#include <vector>
#include <forward_list>
#include <list>
#include <set>
#include <iterator>
#include <array>

using boost::mysql::detail::is_field_view_forward_iterator;
using boost::mysql::detail::is_field_view_collection;
using boost::mysql::field_view;

BOOST_AUTO_TEST_SUITE(test_value_type_traits)

// A custom collection exposing an iterator fulfilling the
// input iterator category, only
template <typename IteratorTag>
class custom_iterator_collection
{
public:
    class iterator
    {
        field_view v_;
    public:
        using value_type = field_view;
        using pointer = field_view*;
        using reference = field_view&;
        using difference_type = std::ptrdiff_t;
        using iterator_category = IteratorTag;

        bool operator==(const iterator&) const noexcept { return true; }
        bool operator!=(const iterator&) const noexcept { return false; }
        iterator& operator++() noexcept { return *this; }
        iterator operator++(int) noexcept { return *this; }
        reference operator*() noexcept { return v_; }
        pointer operator->() noexcept { return &v_; }
    };

    iterator begin() { return iterator(); }
    iterator end() { return iterator(); }
    iterator begin() const { return iterator(); }
    iterator end() const { return iterator(); }
};
using input_iterator_collection = custom_iterator_collection<std::input_iterator_tag>;
using forward_iterator_collection = custom_iterator_collection<std::forward_iterator_tag>;

BOOST_AUTO_TEST_SUITE(test_is_value_forward_iterator)

template <typename T>
void check_is_value_forward_iterator(bool expected)
{
    // Check whether T is accepted or not
    BOOST_TEST(
        is_field_view_forward_iterator<T>::value == expected, 
        boost::typeindex::type_id<T>().pretty_name() + 
            (expected ? " expected true, got false" : " expected false, got true")
    );

    // References to T not accepted
    BOOST_TEST(!is_field_view_forward_iterator<T&>::value,         boost::typeindex::type_id<T&>().pretty_name());
    BOOST_TEST(!is_field_view_forward_iterator<T const&>::value,   boost::typeindex::type_id<T const&>().pretty_name());
    BOOST_TEST(!is_field_view_forward_iterator<T &&>::value,       boost::typeindex::type_id<T &&>().pretty_name());
    BOOST_TEST(!is_field_view_forward_iterator<T const &&>::value, boost::typeindex::type_id<T const &&>().pretty_name());
}

BOOST_AUTO_TEST_CASE(pointers)
{
    check_is_value_forward_iterator<field_view*>(true);
    check_is_value_forward_iterator<const field_view*>(true);
    // value* const is not considered an iterator
}

BOOST_AUTO_TEST_CASE(array_iterators)
{
    check_is_value_forward_iterator<std::array<field_view, 10>::iterator>(true);
    check_is_value_forward_iterator<std::array<field_view, 10>::const_iterator>(true);
    check_is_value_forward_iterator<std::array<const field_view, 10>::iterator>(true);
    check_is_value_forward_iterator<std::array<const field_view, 10>::const_iterator>(true);
    // const std::array<value>::iterator (aka value* const) is not considered an iterator
}

BOOST_AUTO_TEST_CASE(vector_iterator)
{
    check_is_value_forward_iterator<std::vector<field_view>::iterator>(true);
    check_is_value_forward_iterator<std::vector<field_view>::const_iterator>(true);
    check_is_value_forward_iterator<const std::vector<field_view>::iterator>(true);
    check_is_value_forward_iterator<const std::vector<field_view>::const_iterator>(true);
    check_is_value_forward_iterator<std::vector<field_view>::reverse_iterator>(true);
    check_is_value_forward_iterator<std::vector<field_view>::const_reverse_iterator>(true);
    check_is_value_forward_iterator<std::back_insert_iterator<std::vector<field_view>>>(false);
}

BOOST_AUTO_TEST_CASE(forward_list_iterator)
{
    check_is_value_forward_iterator<std::forward_list<field_view>::iterator>(true);
    check_is_value_forward_iterator<std::forward_list<field_view>::const_iterator>(true);
    check_is_value_forward_iterator<const std::forward_list<field_view>::iterator>(true);
    check_is_value_forward_iterator<const std::forward_list<field_view>::const_iterator>(true);
}

BOOST_AUTO_TEST_CASE(list_iterator)
{
    check_is_value_forward_iterator<std::list<field_view>::iterator>(true);
    check_is_value_forward_iterator<std::list<field_view>::const_iterator>(true);
    check_is_value_forward_iterator<const std::list<field_view>::iterator>(true);
    check_is_value_forward_iterator<const std::list<field_view>::const_iterator>(true);
}

BOOST_AUTO_TEST_CASE(set_iterator)
{
    check_is_value_forward_iterator<std::set<field_view>::iterator>(true);
    check_is_value_forward_iterator<std::set<field_view>::const_iterator>(true);
    check_is_value_forward_iterator<const std::set<field_view>::iterator>(true);
    check_is_value_forward_iterator<const std::set<field_view>::const_iterator>(true);
}

BOOST_AUTO_TEST_CASE(custom_collection_iterator)
{
    check_is_value_forward_iterator<input_iterator_collection::iterator>(false);
    check_is_value_forward_iterator<const input_iterator_collection::iterator>(false);
    check_is_value_forward_iterator<forward_iterator_collection::iterator>(true);
    check_is_value_forward_iterator<const forward_iterator_collection::iterator>(true);
}

BOOST_AUTO_TEST_CASE(iterator_wrong_value_type)
{
    check_is_value_forward_iterator<std::vector<const field_view*>::iterator>(false);
    check_is_value_forward_iterator<std::vector<field_view*>::iterator>(false);
    check_is_value_forward_iterator<std::vector<std::reference_wrapper<field_view>>::iterator>(false);
    check_is_value_forward_iterator<std::vector<std::reference_wrapper<const field_view>>::iterator>(false);
    check_is_value_forward_iterator<std::vector<int>::iterator>(false);
    check_is_value_forward_iterator<std::string::iterator>(false);
}

BOOST_AUTO_TEST_CASE(not_an_iterator)
{
    check_is_value_forward_iterator<field_view>(false);
    check_is_value_forward_iterator<int>(false);
    check_is_value_forward_iterator<std::string>(false);
    check_is_value_forward_iterator<std::vector<int>>(false);
}

BOOST_AUTO_TEST_SUITE_END() // test_is_value_forward_iterator

// collections
BOOST_AUTO_TEST_SUITE(test_is_value_collection)

template <typename T>
void check_is_value_collection(bool expected)
{
    auto err_msg = boost::typeindex::type_id<T>().pretty_name() + 
            (expected ? " expected true, got false" : " expected false, got true");

    // Check whether T is accepted or not
    BOOST_TEST(is_field_view_collection<T>::value == expected, err_msg);

    // References to T accepted if T is accepted
    BOOST_TEST(is_field_view_collection<T&>::value == expected,         err_msg);
    BOOST_TEST(is_field_view_collection<T const&>::value == expected,   err_msg);
    BOOST_TEST(is_field_view_collection<T &&>::value == expected,       err_msg);
    BOOST_TEST(is_field_view_collection<T const &&>::value == expected, err_msg);
}

BOOST_AUTO_TEST_CASE(c_arrays)
{
    check_is_value_collection<field_view [10]>(true);
    check_is_value_collection<const field_view[10]>(true);
}

BOOST_AUTO_TEST_CASE(arrays)
{
    check_is_value_collection<std::array<field_view, 10>>(true);
    check_is_value_collection<const std::array<field_view, 10>>(true);
}

BOOST_AUTO_TEST_CASE(vector)
{
    check_is_value_collection<std::vector<field_view>>(true);
    check_is_value_collection<const std::vector<field_view>>(true);
}

BOOST_AUTO_TEST_CASE(forward_list)
{
    check_is_value_collection<std::forward_list<field_view>>(true);
    check_is_value_collection<const std::forward_list<field_view>>(true);
}

BOOST_AUTO_TEST_CASE(list)
{
    check_is_value_collection<std::list<field_view>>(true);
    check_is_value_collection<const std::list<field_view>>(true);
}

BOOST_AUTO_TEST_CASE(set)
{
    check_is_value_collection<std::set<field_view>>(true);
    check_is_value_collection<const std::set<field_view>>(true);
}

BOOST_AUTO_TEST_CASE(custom_collection)
{
    check_is_value_collection<input_iterator_collection>(false);
    check_is_value_collection<forward_iterator_collection>(true);
}

BOOST_AUTO_TEST_CASE(wrong_collection_type)
{
    check_is_value_collection<std::vector<const field_view*>>(false);
    check_is_value_collection<std::vector<field_view*>>(false);
    check_is_value_collection<std::vector<std::reference_wrapper<field_view>>>(false);
    check_is_value_collection<std::vector<std::reference_wrapper<const field_view>>>(false);
    check_is_value_collection<std::vector<int>>(false);
    check_is_value_collection<std::string>(false);
}

BOOST_AUTO_TEST_CASE(not_a_collection)
{
    check_is_value_collection<field_view>(false);
    check_is_value_collection<const field_view>(false);
    check_is_value_collection<const field_view*>(false);
    check_is_value_collection<std::vector<field_view>::iterator>(false);
    check_is_value_collection<int>(false);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END() // test_value_type_traits