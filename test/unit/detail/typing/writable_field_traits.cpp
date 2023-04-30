//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/blob_view.hpp>
#include <boost/mysql/field.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/row.hpp>
#include <boost/mysql/row_view.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/typing/writable_field_traits.hpp>

#include <boost/config.hpp>
#include <boost/optional/optional.hpp>

#include <array>
#include <cstddef>
#include <forward_list>
#include <iosfwd>
#include <iterator>
#include <list>
#include <memory>
#include <set>
#include <string>
#include <vector>
#ifndef BOOST_NO_CXX17_HDR_OPTIONAL
#include <optional>
#endif

#include "custom_allocator.hpp"

using namespace boost::mysql;
using boost::mysql::detail::is_field_view_forward_iterator;
using boost::mysql::detail::is_writable_field;
using boost::mysql::detail::is_writable_field_tuple;
using std::tuple;

namespace {

struct other_traits : std::char_traits<char>
{
};

using string_with_alloc = std::basic_string<char, std::char_traits<char>, test::custom_allocator<char>>;
using string_with_traits = std::basic_string<char, other_traits>;
using blob_with_alloc = std::vector<unsigned char, test::custom_allocator<unsigned char>>;

struct unrelated
{
};

//
// writable_field
//
// field_view accepted
static_assert(is_writable_field<field_view>::value, "");
static_assert(is_writable_field<const field_view>::value, "");
static_assert(is_writable_field<field_view&>::value, "");
static_assert(is_writable_field<const field_view&>::value, "");
static_assert(is_writable_field<field_view&&>::value, "");

// field accepted
static_assert(is_writable_field<field>::value, "");
static_assert(is_writable_field<const field>::value, "");
static_assert(is_writable_field<field&>::value, "");
static_assert(is_writable_field<const field&>::value, "");
static_assert(is_writable_field<field&&>::value, "");

// scalars accepted
static_assert(is_writable_field<std::nullptr_t>::value, "");
static_assert(is_writable_field<unsigned char>::value, "");
static_assert(is_writable_field<char>::value, "");
static_assert(is_writable_field<signed char>::value, "");
static_assert(is_writable_field<short>::value, "");
static_assert(is_writable_field<unsigned short>::value, "");
static_assert(is_writable_field<int>::value, "");
static_assert(is_writable_field<unsigned int>::value, "");
static_assert(is_writable_field<long>::value, "");
static_assert(is_writable_field<unsigned long>::value, "");
static_assert(is_writable_field<float>::value, "");
static_assert(is_writable_field<double>::value, "");
static_assert(is_writable_field<boost::mysql::date>::value, "");
static_assert(is_writable_field<boost::mysql::datetime>::value, "");
static_assert(is_writable_field<boost::mysql::time>::value, "");

// bool accepted
static_assert(is_writable_field<bool>::value, "");

// references to scalars accepted
static_assert(is_writable_field<int&>::value, "");
static_assert(is_writable_field<const int&>::value, "");
static_assert(is_writable_field<int&&>::value, "");

// string types
static_assert(is_writable_field<std::string>::value, "");
static_assert(is_writable_field<std::string&>::value, "");
static_assert(is_writable_field<const std::string&>::value, "");
static_assert(is_writable_field<string_with_alloc>::value, "");
static_assert(is_writable_field<std::string&&>::value, "");
static_assert(is_writable_field<string_view>::value, "");
static_assert(!is_writable_field<string_with_traits>::value, "");
static_assert(!is_writable_field<std::wstring>::value, "");

// blob types accepted
static_assert(is_writable_field<blob>::value, "");
static_assert(is_writable_field<blob&>::value, "");
static_assert(is_writable_field<const blob&>::value, "");
static_assert(is_writable_field<blob_view>::value, "");
static_assert(is_writable_field<blob_with_alloc>::value, "");

// optional types accepted
#ifndef BOOST_NO_CXX17_HDR_OPTIONAL
static_assert(is_writable_field<std::optional<int>>::value, "");
static_assert(is_writable_field<std::optional<std::string>>::value, "");
#endif
static_assert(is_writable_field<boost::optional<string_view>>::value, "");
static_assert(is_writable_field<boost::optional<blob_view>>::value, "");

// optional of other stuff not accepted
static_assert(!is_writable_field<boost::optional<void*>>::value, "");
static_assert(!is_writable_field<boost::optional<unrelated>>::value, "");

// other stuff not accepted
static_assert(!is_writable_field<void*>::value, "");
static_assert(!is_writable_field<field*>::value, "");
static_assert(!is_writable_field<field_view*>::value, "");
static_assert(!is_writable_field<unrelated>::value, "");
static_assert(!is_writable_field<unrelated*>::value, "");

//
// writable_field_tuple
//
// Empty tuples accepted
static_assert(is_writable_field_tuple<tuple<>>::value, "");
static_assert(is_writable_field_tuple<tuple<>&>::value, "");
static_assert(is_writable_field_tuple<const tuple<>&>::value, "");
static_assert(is_writable_field_tuple<tuple<>&&>::value, "");

// Tuples of field likes accepted
static_assert(is_writable_field_tuple<tuple<int, std::string&, const char*>>::value, "");
static_assert(is_writable_field_tuple<tuple<field_view, string_view, int&&>>::value, "");
static_assert(is_writable_field_tuple<tuple<boost::optional<int>, string_view, blob&&>>::value, "");

// References accepted
static_assert(is_writable_field_tuple<tuple<int, float&, std::string&&>&>::value, "");
static_assert(is_writable_field_tuple<const tuple<int, float&, std::string&&>&>::value, "");
static_assert(is_writable_field_tuple<tuple<int, float&, std::string&&>&&>::value, "");

// Tuples of other stuff not accepted
static_assert(!is_writable_field_tuple<tuple<int, std::ostream&>>::value, "");
static_assert(!is_writable_field_tuple<tuple<std::ostream&, char>>::value, "");
static_assert(!is_writable_field_tuple<tuple<std::ostream&, char>&>::value, "");
static_assert(!is_writable_field_tuple<tuple<boost::optional<void*>, char>&>::value, "");

// Non-tuples not accepted
static_assert(!is_writable_field_tuple<int>::value, "");
static_assert(!is_writable_field_tuple<std::array<int, 1>>::value, "");
static_assert(!is_writable_field_tuple<field_view>::value, "");

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
static_assert(is_field_view_forward_iterator<std::array<field_view, 10>::const_iterator>::value, "");

static_assert(is_field_view_forward_iterator<std::array<field, 10>::iterator>::value, "");
static_assert(is_field_view_forward_iterator<std::array<field, 10>::const_iterator>::value, "");

// Vector iterators
static_assert(is_field_view_forward_iterator<std::vector<field_view>::iterator>::value, "");
static_assert(is_field_view_forward_iterator<std::vector<field_view>::const_iterator>::value, "");
static_assert(is_field_view_forward_iterator<std::vector<field_view>::reverse_iterator>::value, "");
static_assert(is_field_view_forward_iterator<std::vector<field_view>::const_reverse_iterator>::value, "");
static_assert(
    is_field_view_forward_iterator<std::vector<std::reference_wrapper<field_view>>::iterator>::value,
    ""
);

static_assert(is_field_view_forward_iterator<std::vector<field>::iterator>::value, "");
static_assert(is_field_view_forward_iterator<std::vector<field>::const_iterator>::value, "");

// forward_list iterators
static_assert(is_field_view_forward_iterator<std::forward_list<field_view>::iterator>::value, "");
static_assert(is_field_view_forward_iterator<std::forward_list<field_view>::const_iterator>::value, "");

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

}  // namespace
