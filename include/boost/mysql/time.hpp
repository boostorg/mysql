//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TIME_HPP
#define BOOST_MYSQL_TIME_HPP

#include <chrono>
#include <cstdint>
#include <ratio>

namespace boost {
namespace mysql {

/// Type representing MySQL `TIME` data type.
using time = std::chrono::duration<std::int64_t, std::micro>;

/// The minimum allowed value for \ref time.
constexpr time min_time = -std::chrono::hours(839);

/// The maximum allowed value for \ref time.
constexpr time max_time = std::chrono::hours(839);

}  // namespace mysql
}  // namespace boost

#endif
