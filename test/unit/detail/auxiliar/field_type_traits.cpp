//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/detail/auxiliar/field_type_traits.hpp>
#include <boost/mysql/field.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/row.hpp>
#include <boost/mysql/row_view.hpp>

#include <array>
#include <cstddef>
#include <forward_list>
#include <iterator>
#include <list>
#include <set>
#include <string>
#include <vector>

using boost::mysql::field;
using boost::mysql::field_view;
using boost::mysql::row;
using boost::mysql::row_view;
using boost::mysql::detail::is_field_view_collection;
using boost::mysql::detail::is_field_view_forward_iterator;

//
// field_view iterator
//

// Pointers. Note that field_view* const is not considered an iterator
static_assert(is_field_view_forward_iterator<field_view*>::value, "");
static_assert(is_field_view_forward_iterator<const field_view*>::value, "");
static_assert(is_field_view_forward_iterator<field*>::value, "");
static_assert(is_field_view_forward_iterator<const field*>::value, "");

// Array iterators
static_assert(is_field_view_forward_iterator<std::array<field_view, 10>::iterator>::value, "");
static_assert(
    is_field_view_forward_iterator<std::array<field_view, 10>::const_iterator>::value,
    ""
);

static_assert(is_field_view_forward_iterator<std::array<field, 10>::iterator>::value, "");
static_assert(is_field_view_forward_iterator<std::array<field, 10>::const_iterator>::value, "");

// Vector iterators
static_assert(is_field_view_forward_iterator<std::vector<field_view>::iterator>::value, "");
static_assert(is_field_view_forward_iterator<std::vector<field_view>::const_iterator>::value, "");
static_assert(is_field_view_forward_iterator<std::vector<field_view>::reverse_iterator>::value, "");
static_assert(
    is_field_view_forward_iterator<std::vector<field_view>::const_reverse_iterator>::value,
    ""
);
static_assert(
    is_field_view_forward_iterator<
        std::vector<std::reference_wrapper<field_view>>::iterator>::value,
    ""
);

static_assert(is_field_view_forward_iterator<std::vector<field>::iterator>::value, "");
static_assert(is_field_view_forward_iterator<std::vector<field>::const_iterator>::value, "");

// forward_list iterators
static_assert(is_field_view_forward_iterator<std::forward_list<field_view>::iterator>::value, "");
static_assert(
    is_field_view_forward_iterator<std::forward_list<field_view>::const_iterator>::value,
    ""
);

static_assert(is_field_view_forward_iterator<std::forward_list<field>::iterator>::value, "");
static_assert(is_field_view_forward_iterator<std::forward_list<field>::const_iterator>::value, "");

// list iterators
static_assert(is_field_view_forward_iterator<std::list<field_view>::iterator>::value, "");
static_assert(is_field_view_forward_iterator<std::list<field_view>::const_iterator>::value, "");

static_assert(is_field_view_forward_iterator<std::list<field>::iterator>::value, "");
static_assert(is_field_view_forward_iterator<std::list<field>::const_iterator>::value, "");

// set iterators
static_assert(is_field_view_forward_iterator<std::set<field_view>::iterator>::value, "");
static_assert(is_field_view_forward_iterator<std::set<field_view>::const_iterator>::value, "");

static_assert(is_field_view_forward_iterator<std::set<field>::iterator>::value, "");
static_assert(is_field_view_forward_iterator<std::set<field>::const_iterator>::value, "");

// row_view iterators
static_assert(is_field_view_forward_iterator<row_view::iterator>::value, "");
static_assert(is_field_view_forward_iterator<row_view::const_iterator>::value, "");

// row iterators
static_assert(is_field_view_forward_iterator<row::iterator>::value, "");
static_assert(is_field_view_forward_iterator<row::const_iterator>::value, "");

// iterators whose reference type doesn't match
static_assert(!is_field_view_forward_iterator<std::vector<field_view*>::iterator>::value, "");
static_assert(!is_field_view_forward_iterator<std::vector<int>::iterator>::value, "");
static_assert(!is_field_view_forward_iterator<std::string::iterator>::value, "");

// types that aren't iterators
static_assert(!is_field_view_forward_iterator<field_view>::value, "");
static_assert(!is_field_view_forward_iterator<int>::value, "");
static_assert(!is_field_view_forward_iterator<std::string>::value, "");
static_assert(!is_field_view_forward_iterator<std::vector<int>>::value, "");

// References to iterators are not accepted
static_assert(!is_field_view_forward_iterator<field_view*&>::value, "");
static_assert(!is_field_view_forward_iterator<row::iterator&>::value, "");
static_assert(!is_field_view_forward_iterator<const row::iterator&>::value, "");

//
// Collections
//

// C arrays
static_assert(is_field_view_collection<field_view[10]>::value, "");
static_assert(is_field_view_collection<const field_view[10]>::value, "");

static_assert(is_field_view_collection<field[10]>::value, "");
static_assert(is_field_view_collection<const field[10]>::value, "");

// std::array
static_assert(is_field_view_collection<std::array<field_view, 10>>::value, "");
static_assert(is_field_view_collection<const std::array<field_view, 10>>::value, "");

static_assert(is_field_view_collection<std::array<field, 10>>::value, "");
static_assert(is_field_view_collection<const std::array<field, 10>>::value, "");

// vector
static_assert(is_field_view_collection<std::vector<field_view>>::value, "");
static_assert(is_field_view_collection<const std::vector<field_view>>::value, "");

static_assert(is_field_view_collection<std::vector<field>>::value, "");
static_assert(is_field_view_collection<const std::vector<field>>::value, "");

// forward_list
static_assert(is_field_view_collection<std::forward_list<field_view>>::value, "");
static_assert(is_field_view_collection<const std::forward_list<field_view>>::value, "");

static_assert(is_field_view_collection<std::forward_list<field>>::value, "");
static_assert(is_field_view_collection<const std::forward_list<field>>::value, "");

// list
static_assert(is_field_view_collection<std::list<field_view>>::value, "");
static_assert(is_field_view_collection<const std::list<field_view>>::value, "");

static_assert(is_field_view_collection<std::list<field>>::value, "");
static_assert(is_field_view_collection<const std::list<field>>::value, "");

// set
static_assert(is_field_view_collection<std::set<field_view>>::value, "");
static_assert(is_field_view_collection<const std::set<field_view>>::value, "");

static_assert(is_field_view_collection<std::set<field>>::value, "");
static_assert(is_field_view_collection<const std::set<field>>::value, "");

// row_view
static_assert(is_field_view_collection<row_view>::value, "");
static_assert(is_field_view_collection<const row_view>::value, "");

// row
static_assert(is_field_view_collection<row>::value, "");
static_assert(is_field_view_collection<const row>::value, "");

// wrong collection type
static_assert(!is_field_view_collection<std::vector<const field_view*>>::value, "");
static_assert(!is_field_view_collection<std::vector<field_view*>>::value, "");
static_assert(!is_field_view_collection<std::vector<int>>::value, "");
static_assert(!is_field_view_collection<std::string>::value, "");

// not a collection
static_assert(!is_field_view_collection<field_view>::value, "");
static_assert(!is_field_view_collection<const field_view>::value, "");
static_assert(!is_field_view_collection<const field_view*>::value, "");
static_assert(!is_field_view_collection<std::vector<field_view>::iterator>::value, "");
static_assert(!is_field_view_collection<int>::value, "");
