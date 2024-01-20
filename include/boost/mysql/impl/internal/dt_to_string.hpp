//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_DT_TO_STRING_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_DT_TO_STRING_HPP

#include <boost/mysql/time.hpp>

#include <boost/assert.hpp>
#include <boost/core/span.hpp>

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <system_error>

// gcc-11+ issues spurious warnings about to_chars
#if BOOST_GCC >= 110000
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif

namespace boost {
namespace mysql {
namespace detail {

// Helpers
inline char* call_to_chars(char* begin, char* end, std::uint64_t value) noexcept
{
    auto r = std::to_chars(begin, end, value);
    BOOST_ASSERT(r.ec == std::errc());
    return r.ptr;
}

inline char* write_pad2(char* begin, char* end, std::uint64_t value) noexcept
{
    if (value < 10u)
        *begin++ = '0';
    return call_to_chars(begin, end, value);
}

inline char* write_pad4(char* begin, char* end, std::uint64_t value) noexcept
{
    for (auto l : {1000u, 100u, 10u})
    {
        if (value < l)
            *begin++ = '0';
    }
    return call_to_chars(begin, end, value);
}

inline char* write_pad6(char* begin, char* end, std::uint64_t value) noexcept
{
    for (auto l : {100000u, 10000u, 1000u, 100u, 10u})
    {
        if (value < l)
            *begin++ = '0';
    }
    return call_to_chars(begin, end, value);
}

inline std::size_t date_to_string(
    std::uint16_t year,
    std::uint8_t month,
    std::uint8_t day,
    span<char, 32> output
) noexcept
{
    // Worst-case output is 14 chars, extra space just in case

    // Iterators
    char* it = output.data();
    char* end = it + output.size();

    // Year
    it = write_pad4(it, end, year);

    // Month
    *it++ = '-';
    it = write_pad2(it, end, month);

    // Day
    *it++ = '-';
    it = write_pad2(it, end, day);

    // Done
    return it - output.data();
}

inline std::size_t datetime_to_string(
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

    // Iterators
    char* it = output.data();
    char* end = it + output.size();

    // Date
    it += date_to_string(year, month, day, span<char, 32>(it, 32));

    // Hour
    *it++ = ' ';
    it = write_pad2(it, end, hour);

    // Minutes
    *it++ = ':';
    it = write_pad2(it, end, minute);

    // Seconds
    *it++ = ':';
    it = write_pad2(it, end, second);

    // Microseconds
    *it++ = '.';
    it = write_pad6(it, end, microsecond);

    // Done
    return it - output.data();
}

inline std::size_t time_to_string(::boost::mysql::time value, span<char, 64> output) noexcept
{
    // Worst-case output is 34 chars, extra space just in case

    // Values. Note that std::abs(mysql_time::min()) invokes UB because of
    // signed integer overflow
    auto total_count = value == (::boost::mysql::time::min)()
                           ? static_cast<std::uint64_t>((::boost::mysql::time::min)().count()) + 1u
                           : static_cast<std::uint64_t>(std::abs(value.count()));

    auto num_micros = total_count % 1000000u;
    total_count /= 1000000u;

    auto num_secs = total_count % 60u;
    total_count /= 60u;

    auto num_mins = total_count % 60u;
    total_count /= 60u;

    auto num_hours = total_count;

    // Iterators
    char* it = output.data();
    char* end = it + output.size();

    // Sign
    if (value.count() < 0)
        *it++ = '-';

    // Hours
    it = write_pad2(it, end, num_hours);

    // Minutes
    *it++ = ':';
    it = write_pad2(it, end, num_mins);

    // Seconds
    *it++ = ':';
    it = write_pad2(it, end, num_secs);

    // Microseconds
    *it++ = '.';
    it = write_pad6(it, end, num_micros);

    // Done
    return it - output.data();
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#if BOOST_GCC >= 110000
#pragma GCC diagnostic pop
#endif

#endif
