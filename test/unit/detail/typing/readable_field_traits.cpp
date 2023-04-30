//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/blob.hpp>
#include <boost/mysql/blob_view.hpp>
#include <boost/mysql/field.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/typing/readable_field_traits.hpp>

#include <boost/optional/optional.hpp>
#include <boost/test/unit_test.hpp>

#include <cstddef>
#include <string>

#include "creation/create_meta.hpp"
#include "custom_allocator.hpp"
#include "printing.hpp"

#ifndef BOOST_NO_CXX17_HDR_OPTIONAL
#include <optional>
#endif

using namespace boost::mysql;
using namespace boost::mysql::test;
using detail::is_readable_field;

namespace {

BOOST_AUTO_TEST_SUITE(test_readable_field_traits)

//
// readable_field
//

struct other_traits : std::char_traits<char>
{
};
using string_with_alloc = std::basic_string<char, std::char_traits<char>, custom_allocator<char>>;
using string_with_traits = std::basic_string<char, other_traits>;
using blob_with_alloc = std::vector<unsigned char, custom_allocator<unsigned char>>;

struct unrelated
{
};

// scalars. char not supported directly, since may or may not be signed
static_assert(is_readable_field<unsigned char>::value, "");
static_assert(is_readable_field<signed char>::value, "");
static_assert(is_readable_field<short>::value, "");
static_assert(is_readable_field<unsigned short>::value, "");
static_assert(is_readable_field<int>::value, "");
static_assert(is_readable_field<unsigned int>::value, "");
static_assert(is_readable_field<long>::value, "");
static_assert(is_readable_field<unsigned long>::value, "");
static_assert(is_readable_field<std::int8_t>::value, "");
static_assert(is_readable_field<std::uint8_t>::value, "");
static_assert(is_readable_field<std::int16_t>::value, "");
static_assert(is_readable_field<std::uint16_t>::value, "");
static_assert(is_readable_field<std::int32_t>::value, "");
static_assert(is_readable_field<std::uint32_t>::value, "");
static_assert(is_readable_field<std::int64_t>::value, "");
static_assert(is_readable_field<std::uint64_t>::value, "");
static_assert(is_readable_field<float>::value, "");
static_assert(is_readable_field<double>::value, "");
static_assert(is_readable_field<boost::mysql::date>::value, "");
static_assert(is_readable_field<boost::mysql::datetime>::value, "");
static_assert(is_readable_field<boost::mysql::time>::value, "");
static_assert(is_readable_field<bool>::value, "");

// string types
static_assert(is_readable_field<std::string>::value, "");
static_assert(is_readable_field<string_with_alloc>::value, "");
static_assert(!is_readable_field<string_with_traits>::value, "");
static_assert(!is_readable_field<std::wstring>::value, "");
static_assert(!is_readable_field<string_view>::value, "");

// blob types
static_assert(is_readable_field<blob>::value, "");
static_assert(is_readable_field<blob_with_alloc>::value, "");
static_assert(!is_readable_field<blob_view>::value, "");

// references not accepted
static_assert(!is_readable_field<int&>::value, "");
static_assert(!is_readable_field<const int&>::value, "");
static_assert(!is_readable_field<int&&>::value, "");
static_assert(!is_readable_field<const int&>::value, "");
static_assert(!is_readable_field<std::string&>::value, "");

// optionals
#ifndef BOOST_NO_CXX17_HDR_OPTIONAL
static_assert(is_readable_field<std::optional<int>>::value, "");
static_assert(is_readable_field<std::optional<std::string>>::value, "");
#endif
static_assert(is_readable_field<boost::optional<blob>>::value, "");
static_assert(is_readable_field<boost::optional<datetime>>::value, "");
static_assert(!is_readable_field<boost::optional<void*>>::value, "");
static_assert(!is_readable_field<boost::optional<unrelated>>::value, "");
static_assert(is_readable_field<non_null<float>>::value, "");
static_assert(is_readable_field<non_null<std::string>>::value, "");
static_assert(!is_readable_field<non_null<void*>>::value, "");

// other types not accepted
static_assert(!is_readable_field<std::nullptr_t>::value, "");
static_assert(!is_readable_field<field_view>::value, "");
static_assert(!is_readable_field<field>::value, "");
static_assert(!is_readable_field<const char*>::value, "");
static_assert(!is_readable_field<void*>::value, "");
static_assert(!is_readable_field<unrelated>::value, "");
static_assert(!is_readable_field<const field_view*>::value, "");

// meta_check:
//     all valid
//     all invalid: error message matches
// parse:
//     make parse errors non-fatal (?)
//     check for all valid types
//     check for all invalid types

BOOST_AUTO_TEST_SUITE_END()

}  // namespace
