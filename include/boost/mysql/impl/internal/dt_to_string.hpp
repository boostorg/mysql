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
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <system_error>

namespace boost {
namespace mysql {
namespace detail {

// Helpers
template <class IntType>
inline char* call_to_chars(char* begin, char* end, IntType value) noexcept
{
    auto r = std::to_chars(begin, end, value);
    BOOST_ASSERT(r.ec == std::errc());
    return r.ptr;
}

template <class IntType>
inline char* write_pad2(char* begin, char* end, IntType value) noexcept
{
    if (value < 10)
        *begin++ = '0';
    return call_to_chars(begin, end, value);
}

template <class IntType>
inline char* write_pad4(char* begin, char* end, IntType value) noexcept
{
    for (long l : {1000, 100, 10})
    {
        if (value < l)
            *begin++ = '0';
    }
    return call_to_chars(begin, end, value);
}

template <class IntType>
inline char* write_pad6(char* begin, char* end, IntType value) noexcept
{
    for (long l : {100000, 10000, 1000, 100, 10})
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
    it = write_pad4(it, end, static_cast<unsigned>(year));

    // Month
    *it++ = '-';
    it = write_pad2(it, end, static_cast<unsigned>(month));

    // Day
    *it++ = '-';
    it = write_pad2(it, end, static_cast<unsigned>(day));

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
    it = write_pad2(it, end, static_cast<unsigned>(hour));

    // Minutes
    *it++ = ':';
    it = write_pad2(it, end, static_cast<unsigned>(minute));

    // Seconds
    *it++ = ':';
    it = write_pad2(it, end, static_cast<unsigned>(second));

    // Microseconds
    *it++ = '.';
    it = write_pad6(it, end, microsecond);

    // Done
    return it - output.data();
}

inline std::size_t time_to_string(::boost::mysql::time value, span<char, 64> output) noexcept
{
    // Worst-case output is 26 chars, extra space just in case

    // Values
    auto total_count = std::abs(value.count());

    auto num_micros = static_cast<unsigned>(total_count % 1000000);
    total_count /= 1000000;

    auto num_secs = static_cast<unsigned>(total_count % 60);
    total_count /= 60;

    auto num_mins = static_cast<unsigned>(total_count % 60);
    total_count /= 60;

    auto num_hours = static_cast<unsigned>(total_count);

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

#endif
