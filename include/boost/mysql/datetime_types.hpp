//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DATETIME_TYPES_HPP
#define BOOST_MYSQL_DATETIME_TYPES_HPP

#include <boost/config.hpp>

#include <chrono>

namespace boost {
namespace mysql {

/**
 * \brief Duration representing a day (24 hours).
 * \details Suitable to represent the range of dates MySQL offers.
 * May differ in representation from `std::chrono::days` in C++20.
 */
using days = std::chrono::duration<int, std::ratio<3600 * 24>>;

/// Type representing MySQL `__DATE__` data type.
using date = std::chrono::time_point<std::chrono::system_clock, days>;

/// Type representing MySQL `__DATETIME__` and `__TIMESTAMP__` data types.
using datetime = std::chrono::time_point<std::chrono::system_clock, std::chrono::microseconds>;

/// Type representing MySQL `__TIME__` data type.
using time = std::chrono::microseconds;

/// The minimum allowed value for [reflink date] (0000-01-01).
BOOST_CXX14_CONSTEXPR const date min_date{days(-719528)};

/// The maximum allowed value for [reflink date] (9999-12-31).
BOOST_CXX14_CONSTEXPR const date max_date{days(2932896)};

/// The minimum allowed value for [reflink datetime].
BOOST_CXX14_CONSTEXPR const datetime min_datetime = min_date;

/// The maximum allowed value for [reflink datetime].
BOOST_CXX14_CONSTEXPR const datetime max_datetime =
    max_date + std::chrono::hours(24) - std::chrono::microseconds(1);

/// The minimum allowed value for [reflink time].
constexpr time min_time = -std::chrono::hours(839);

/// The maximum allowed value for [reflink time].
constexpr time max_time = std::chrono::hours(839);

}  // namespace mysql
}  // namespace boost

#endif
