//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PROTOCOL_DATE_HPP
#define BOOST_MYSQL_DETAIL_PROTOCOL_DATE_HPP

// All these algorithms have been taken from:
// http://howardhinnant.github.io/date_algorithms.html

#include <cstdint>
#include <boost/config.hpp>

namespace boost {
namespace mysql {
namespace detail {

struct year_month_day
{
    int years;
    unsigned month;
    unsigned day;
};

BOOST_CXX14_CONSTEXPR inline bool is_valid(const year_month_day& ymd) noexcept;
BOOST_CXX14_CONSTEXPR inline int ymd_to_days(const year_month_day& ymd) noexcept;
BOOST_CXX14_CONSTEXPR inline year_month_day days_to_ymd(int num_days) noexcept;

} // detail
} // mysql
} // boost

#include <boost/mysql/detail/protocol/impl/date.hpp>

#endif
