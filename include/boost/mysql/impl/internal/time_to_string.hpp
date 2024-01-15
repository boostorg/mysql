//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_TIME_TO_STRING_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_TIME_TO_STRING_HPP

#include <boost/mysql/time.hpp>

#include <boost/assert.hpp>
#include <boost/core/span.hpp>

#include <charconv>
#include <chrono>
#include <cstddef>
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
inline char* write_pad6(char* begin, char* end, IntType value) noexcept
{
    for (long l : {100000, 10000, 1000, 100, 10})
    {
        if (value < l)
            *begin++ = '0';
    }
    return call_to_chars(begin, end, value);
}

inline std::size_t time_to_string(::boost::mysql::time value, span<char, 64> output) noexcept
{
    // Worst-case output is 26 chars, extra space just in case
    namespace chrono = std::chrono;

    // Values
    auto micros = value % chrono::seconds(1);
    auto secs = duration_cast<chrono::seconds>(value % chrono::minutes(1) - micros);
    auto mins = duration_cast<chrono::minutes>(value % chrono::hours(1) - secs);
    auto hours = duration_cast<chrono::hours>(value - mins);

    auto num_micros = std::abs(micros.count());
    auto num_secs = std::abs(secs.count());
    auto num_mins = std::abs(mins.count());
    auto num_hours = std::abs(hours.count());

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
