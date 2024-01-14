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

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>

namespace boost {
namespace mysql {
namespace detail {

inline std::size_t time_to_string(::boost::mysql::time value, span<char, 64> output) noexcept
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
    BOOST_ASSERT(res > 0 && res <= 64);
    return static_cast<std::size_t>(res);
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
