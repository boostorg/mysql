//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PROTOCOL_PROTOCOL_TYPES_HPP
#define BOOST_MYSQL_DETAIL_PROTOCOL_PROTOCOL_TYPES_HPP

#include "boost/mysql/detail/protocol/value_holder.hpp"
#include <cstdint>
#include <string_view>
#include <array>
#include <vector>
#include <cstring>

namespace boost {
namespace mysql {
namespace detail {

struct int1 : value_holder<std::uint8_t> { using value_holder::value_holder; };
struct int2 : value_holder<std::uint16_t> { using value_holder::value_holder; };
struct int3 : value_holder<std::uint32_t> { using value_holder::value_holder; };
struct int4 : value_holder<std::uint32_t> { using value_holder::value_holder; };
struct int6 : value_holder<std::uint64_t> { using value_holder::value_holder; };
struct int8 : value_holder<std::uint64_t> { using value_holder::value_holder; };
struct int1_signed : value_holder<std::int8_t> { using value_holder::value_holder; };
struct int2_signed : value_holder<std::int16_t> { using value_holder::value_holder; };
struct int4_signed : value_holder<std::int32_t> { using value_holder::value_holder; };
struct int8_signed : value_holder<std::int64_t> { using value_holder::value_holder; };
struct int_lenenc : value_holder<std::uint64_t> { using value_holder::value_holder; };

template <std::size_t size>
using string_fixed = std::array<char, size>;

struct string_null : value_holder<std::string_view> { using value_holder::value_holder; };
struct string_eof : value_holder<std::string_view> { using value_holder::value_holder; };
struct string_lenenc : value_holder<std::string_view> { using value_holder::value_holder; };


} // detail
} // mysql
} // boost

#endif
