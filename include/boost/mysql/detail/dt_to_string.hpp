//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_DT_TO_STRING_HPP
#define BOOST_MYSQL_DETAIL_DT_TO_STRING_HPP

#include <boost/mysql/detail/config.hpp>

#include <boost/core/span.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace boost {
namespace mysql {
namespace detail {

BOOST_MYSQL_DECL std::size_t format_date(
    std::uint16_t year,
    std::uint8_t month,
    std::uint8_t day,
    span<char, 32> output
) noexcept;

BOOST_MYSQL_DECL std::size_t format_datetime(
    std::uint16_t year,
    std::uint8_t month,
    std::uint8_t day,
    std::uint8_t hour,
    std::uint8_t minute,
    std::uint8_t second,
    std::uint32_t microsecond,
    span<char, 64> output
) noexcept;

BOOST_MYSQL_DECL std::size_t format_time(std::chrono::microseconds t, span<char, 64> output) noexcept;

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#ifdef BOOST_MYSQL_HEADER_ONLY
#include <boost/mysql/impl/dt_to_string.ipp>
#endif

#endif
