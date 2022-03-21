//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PROTOCOL_PROTOCOL_TYPES_HPP
#define BOOST_MYSQL_DETAIL_PROTOCOL_PROTOCOL_TYPES_HPP

#include <boost/utility/string_view.hpp>
#include <cstdint>
#include <array>
#include <vector>
#include <cstring>
#include <type_traits>

namespace boost {
namespace mysql {
namespace detail {

template <class T>
struct value_holder
{
    using value_type = T;

    static_assert(std::is_nothrow_default_constructible<T>::value,
        "value_holder<T> should be nothrow default constructible");
    static_assert(std::is_nothrow_move_constructible<T>::value,
        "value_holder<T> should be nothrow move constructible");

    value_type value;

    value_holder() noexcept : value{} {};

    explicit constexpr value_holder(value_type v) noexcept : value(v) {}

    constexpr bool operator==(const value_holder<T>& rhs) const { return value == rhs.value; }
    constexpr bool operator!=(const value_holder<T>& rhs) const { return value != rhs.value; }
};

struct int3 : value_holder<std::uint32_t> { using value_holder::value_holder; };
struct int_lenenc : value_holder<std::uint64_t> { using value_holder::value_holder; };

struct string_null : value_holder<boost::string_view> { using value_holder::value_holder; };
struct string_eof : value_holder<boost::string_view> { using value_holder::value_holder; };
struct string_lenenc : value_holder<boost::string_view> { using value_holder::value_holder; };

template <std::size_t size>
using string_fixed = std::array<char, size>;


} // detail
} // mysql
} // boost

#endif
