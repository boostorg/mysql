//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_DT_TO_STRING_IPP
#define BOOST_MYSQL_IMPL_DT_TO_STRING_IPP

#pragma once

#include <boost/mysql/detail/dt_to_string.hpp>

#include <boost/assert.hpp>

#include <cstddef>
#include <cstdio>

std::size_t boost::mysql::detail::format_date(
    std::uint16_t year,
    std::uint8_t month,
    std::uint8_t day,
    span<char, 32> output
) noexcept
{
    // Worst-case output is 14 chars, extra space just in case
    int res = snprintf(
        output.data(),
        output.size(),
        "%04u-%02u-%02u",
        static_cast<unsigned>(year),
        static_cast<unsigned>(month),
        static_cast<unsigned>(day)
    );
    BOOST_ASSERT(res > 0);
    return static_cast<std::size_t>(res);
}

std::size_t boost::mysql::detail::format_datetime(
    std::uint16_t year,
    std::uint8_t month,
    std::uint8_t day,
    std::uint8_t hour,
    std::uint8_t minute,
    std::uint8_t second,
    std::uint32_t microsecond,
    span<char, 64> output
) noexcept
{
    // Worst-case output is 37 chars, extra space just in case
    int res = snprintf(
        output.data(),
        output.size(),
        "%04u-%02u-%02u %02d:%02u:%02u.%06u",
        static_cast<unsigned>(year),
        static_cast<unsigned>(month),
        static_cast<unsigned>(day),
        static_cast<unsigned>(hour),
        static_cast<unsigned>(minute),
        static_cast<unsigned>(second),
        static_cast<unsigned>(microsecond)
    );
    BOOST_ASSERT(res > 0);
    return static_cast<std::size_t>(res);
}

std::size_t boost::mysql::detail::format_time(std::chrono::microseconds value, span<char, 64> output) noexcept
{
    // Worst-case output is 26 chars, extra space just in case
    using namespace std::chrono;
    const char* sign = value < microseconds(0) ? "-" : "";
    auto num_micros = value % seconds(1);
    auto num_secs = duration_cast<seconds>(value % minutes(1) - num_micros);
    auto num_mins = duration_cast<minutes>(value % hours(1) - num_secs);
    auto num_hours = duration_cast<hours>(value - num_mins);

    int res = snprintf(
        output.data(),
        output.size(),
        "%s%02d:%02u:%02u.%06u",
        sign,
        static_cast<int>(std::abs(num_hours.count())),
        static_cast<unsigned>(std::abs(num_mins.count())),
        static_cast<unsigned>(std::abs(num_secs.count())),
        static_cast<unsigned>(std::abs(num_micros.count()))
    );
    BOOST_ASSERT(res > 0);
    return static_cast<std::size_t>(res);
}

#endif
