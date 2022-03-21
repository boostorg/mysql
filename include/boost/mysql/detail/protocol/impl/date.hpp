//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_DATE_HPP
#define BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_DATE_HPP

#pragma once

#include <boost/mysql/detail/protocol/date.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>
#include <cassert>

namespace boost {
namespace mysql {
namespace detail {

constexpr bool is_leap(int y) noexcept
{
    return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0);
}

BOOST_CXX14_CONSTEXPR inline unsigned last_month_day(int y, unsigned m) noexcept
{
    constexpr unsigned char a [] =
        {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return m != 2 || !is_leap(y) ? a[m-1] : 29u;
}

} // detail
} // mysql
} // boost

BOOST_CXX14_CONSTEXPR inline bool boost::mysql::detail::is_valid(
    const year_month_day& ymd
) noexcept
{
    return
        ymd.years >= 0 &&
        ymd.years <= static_cast<int>(max_year) &&
        ymd.month > 0 &&
        ymd.month <= max_month &&
        ymd.day > 0 &&
        ymd.day <= last_month_day(ymd.years, ymd.month);
}

BOOST_CXX14_CONSTEXPR inline int boost::mysql::detail::ymd_to_days(
    const year_month_day& ymd
) noexcept
{
    assert(is_valid(ymd));
    int y = ymd.years;
    const int m = ymd.month;
    const int d = ymd.day;
    y -= m <= 2;
    const int era = (y >= 0 ? y : y-399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);      // [0, 399]
    const unsigned doy = (153*(m + (m > 2 ? -3 : 9)) + 2)/5 + d-1;  // [0, 365]
    const unsigned doe = yoe * 365 + yoe/4 - yoe/100 + doy;         // [0, 146096]
    return era * 146097 + static_cast<int>(doe) - 719468;
}

BOOST_CXX14_CONSTEXPR inline boost::mysql::detail::year_month_day
boost::mysql::detail::days_to_ymd(
    int num_days
) noexcept
{
    num_days += 719468;
    const int era = (num_days >= 0 ? num_days : num_days - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(num_days - era * 146097);          // [0, 146096]
    const unsigned yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;  // [0, 399]
    const int y = static_cast<int>(yoe) + era * 400;
    const unsigned doy = doe - (365*yoe + yoe/4 - yoe/100);                // [0, 365]
    const unsigned mp = (5*doy + 2)/153;                                   // [0, 11]
    const unsigned d = doy - (153*mp+2)/5 + 1;                             // [1, 31]
    const unsigned m = mp + (mp < 10 ? 3 : -9);                            // [1, 12]
    return year_month_day{y + (m <= 2), m, d};
}





#endif
