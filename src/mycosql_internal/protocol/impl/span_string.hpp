//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_IMPL_SPAN_STRING_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_IMPL_SPAN_STRING_HPP

#include <cstdint>
#include <span>
#include <string_view>

namespace boost {
namespace mysql {
namespace detail {

inline std::string_view to_string(std::span<const std::uint8_t> v)
{
    return std::string_view(reinterpret_cast<const char*>(v.data()), v.size());
}
inline std::span<const std::uint8_t> to_span(std::string_view v)
{
    return std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(v.data()), v.size());
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
