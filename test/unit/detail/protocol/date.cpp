//
// Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/detail/protocol/date.hpp>
#include "test_common.hpp"
#include <algorithm>
#include <limits>
#include <cstdio>
#include <array>

using namespace boost::unit_test;
using namespace boost::mysql::test;
using namespace boost::mysql::detail;

// These tests are very extensive in range. Making them parameterized
// proves very runtime expensive. We rather use plain loops.

BOOST_AUTO_TEST_SUITE(test_date)

// Helpers
constexpr int leap_years [] = {
    1804, 1808, 1812, 1816, 1820, 1824, 1828, 1832, 1836, 1840, 1844, 1848,
    1852, 1856, 1860, 1864, 1868, 1872, 1876, 1880, 1884, 1888, 1892, 1896,
    1904, 1908, 1912, 1916, 1920, 1924, 1928, 1932, 1936, 1940, 1944, 1948,
    1952, 1956, 1960, 1964, 1968, 1972, 1976, 1980, 1984, 1988, 1992, 1996,
    2000, 2004, 2008, 2012, 2016, 2020, 2024, 2028, 2032, 2036, 2040, 2044,
    2048, 2052, 2056, 2060, 2064, 2068, 2072, 2076, 2080, 2084, 2088, 2092,
    2096, 2104, 2108, 2112, 2116, 2120, 2124, 2128, 2132, 2136, 2140, 2144,
    2148, 2152, 2156, 2160, 2164, 2168, 2172, 2176, 2180, 2184, 2188, 2192,
    2196, 2204
};

bool is_leap_year(int y)
{
    return std::binary_search(std::begin(leap_years), std::end(leap_years), y);
}

unsigned last_day_of_month(unsigned month) // doesn't take leap years into account
{
    constexpr unsigned last_month_days [] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    assert(month >= 1 && month <= 12);
    return last_month_days[month-1];
}

std::array<char, 32> date_to_string(int year, unsigned month, unsigned day)
{
    std::array<char, 32> res {};
    snprintf(res.data(), 32, "%4d-%2u-%2u", year, month, day);
    return res;
}

// is_valid
BOOST_AUTO_TEST_CASE(is_valid_ymd_valid_year_month_invalid_day)
{
    for (int year = 1804; year <= 2204; ++year)
    {
        bool leap = is_leap(year);
        for (unsigned month = 1; month <= 12; ++month)
        {
            unsigned last_month_day =
                month == 2 && leap ? 29u : last_day_of_month(month);
            for (unsigned day = 1; day <= 32; ++day)
            {
                auto test_name = date_to_string(year, month, day);
                year_month_day ymd {year, month, day};
                BOOST_TEST(is_valid(ymd) == (day <= last_month_day), test_name.data());
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(is_valid_ymd_year_out_of_mysql_range)
{
    BOOST_TEST(!is_valid(year_month_day{-1, 1, 1}));
    BOOST_TEST(!is_valid(year_month_day{10000, 1, 1}));
    BOOST_TEST(!is_valid(year_month_day{std::numeric_limits<int>::min(), 1, 1}));
    BOOST_TEST(!is_valid(year_month_day{std::numeric_limits<int>::max(), 1, 1}));
}

BOOST_AUTO_TEST_CASE(is_valid_ymd_year_in_mysql_range)
{
    BOOST_TEST(is_valid(year_month_day{0, 1, 1}));
    BOOST_TEST(is_valid(year_month_day{9999, 1, 1}));
}

BOOST_AUTO_TEST_CASE(is_valid_ymd_month_out_of_range)
{
    BOOST_TEST(!is_valid(year_month_day{2010, 0, 1}));
    BOOST_TEST(!is_valid(year_month_day{2010, 13, 1}));
    BOOST_TEST(!is_valid(year_month_day{2010, std::numeric_limits<unsigned>::max(), 1}));
}

// ymd_to_days, days_to_ymd
// Helper function that actually performs the assertions for us
void ymd_years_test(int year, unsigned month, unsigned day, int num_days)
{
    auto test_name = date_to_string(year, month, day);
    BOOST_TEST_CHECKPOINT(test_name.data());

    year_month_day ymd {year, month, day};

    BOOST_TEST(is_valid(ymd), test_name.data());
    BOOST_TEST(ymd_to_days(ymd) == num_days, test_name.data());
    auto actual_ymd = days_to_ymd(num_days);
    BOOST_TEST(actual_ymd.day == day, test_name.data());
    BOOST_TEST(actual_ymd.month == month, test_name.data());
    BOOST_TEST(actual_ymd.years == year, test_name.data());
}

BOOST_AUTO_TEST_CASE(ymd_to_days_days_to_ymd)
{
    // Starting from 1970, going up
    int num_days = 0;

    for (int year = 1970; year <= 2204; ++year)
    {
        for (unsigned month = 1; month <= 12; ++month)
        {
            unsigned last_month_day =
                month == 2 && is_leap_year(year) ? 29u : last_day_of_month(month);
            for (unsigned day = 1; day <= last_month_day; ++day)
            {
                ymd_years_test(year, month, day, num_days++);
            }
        }
    }

    // Starting from 1970, going down
    num_days = -1;

    for (int year = 1969; year >= 1804; --year)
    {
        for (unsigned month = 12; month >= 1; --month)
        {
            unsigned last_month_day =
                month == 2 && is_leap_year(year) ? 29u : last_day_of_month(month);
            for (unsigned day = last_month_day; day >= 1; --day)
            {
                ymd_years_test(year, month, day, num_days--);
            }
        }
    }
}

BOOST_AUTO_TEST_SUITE_END() // test_date

