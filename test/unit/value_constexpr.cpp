//
// Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "boost/mysql/value.hpp"
#include "test_common.hpp"
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_suite.hpp>
#include <cstdint>

using boost::mysql::value;
using boost::mysql::null_t;
using boost::mysql::datetime;
using boost::mysql::date;
using boost::mysql::days;
using vt = boost::mysql::value::variant_type;

#ifndef BOOST_NO_CXX14_CONSTEXPR

BOOST_AUTO_TEST_SUITE(test_value_constexpr)

#ifndef _MSC_VER // for some reason, MSVC doesn't like these tests

BOOST_AUTO_TEST_CASE(null_type)
{
    constexpr value v;
    constexpr value v2 (nullptr);
    static_assert(v.is_null(), "");
    static_assert(v.is<null_t>(), "");
    static_assert(v.is_convertible_to<null_t>(), "");
    static_assert(v.to_variant() == vt(null_t()), "");
    static_assert(v == v2, "");
    static_assert(v != value(10), "");
    static_assert(v < value(10), "");
    static_assert(v <= value(10), "");
    static_assert(v >= v2, "");

    #ifndef BOOST_NO_CXX17_HDR_OPTIONAL
    static_assert(v.get_std_optional<null_t>(), "");
    #endif
}

#endif

BOOST_AUTO_TEST_CASE(int64_t_type)
{
    constexpr value v (std::int64_t(60));
    constexpr value v2 (-4); // from other int type
    static_assert(!v.is_null(), "");
    static_assert(v.is<std::int64_t>(), "");
    static_assert(v.is_convertible_to<std::int64_t>(), "");
    static_assert(v.is_convertible_to<std::uint64_t>(), "");
    static_assert(v.to_variant() == vt(std::int64_t(60)), "");
    static_assert(v == v, "");
    static_assert(v != v2, "");
    static_assert(v < value(boost::mysql::time(9999)), "");
    static_assert(v <= value(boost::mysql::time(9999)), "");
    static_assert(v > value(std::int64_t(0)), "");
    static_assert(v >= value(std::int64_t(0)), "");

    #ifndef BOOST_NO_CXX17_HDR_OPTIONAL
    static_assert(v.get_std_optional<std::int64_t>(), "");
    #endif
}

BOOST_AUTO_TEST_CASE(uint64_t_type)
{
    constexpr value v (std::uint64_t(60));
    static_assert(!v.is_null(), "");
    static_assert(v.is<std::uint64_t>(), "");
    static_assert(v.is_convertible_to<std::int64_t>(), "");
    static_assert(v.is_convertible_to<std::uint64_t>(), "");
    static_assert(v.to_variant() == vt(std::uint64_t(60)), "");
    static_assert(v == v, "");
    static_assert(v != value(std::int64_t(0)), "");
    static_assert(v < value(boost::mysql::time(9999)), "");
    static_assert(v <= value(boost::mysql::time(9999)), "");
    static_assert(v > value(std::int64_t(0)), "");
    static_assert(v >= value(std::int64_t(0)), "");

    #ifndef BOOST_NO_CXX17_HDR_OPTIONAL
    static_assert(v.get_std_optional<std::uint64_t>(), "");
    #endif
}

BOOST_AUTO_TEST_CASE(string_view_type)
{
    constexpr value v (boost::string_view("test", 4));
    static_assert(!v.is_null(), "");
    static_assert(v.is<boost::string_view>(), "");
    static_assert(v.is_convertible_to<boost::string_view>(), "");
    // Equality is not constexpr for strings 
    static_assert(v != value(std::int64_t(0)), "");
    static_assert(v < value(boost::mysql::time(9999)), "");
    static_assert(v <= value(boost::mysql::time(9999)), "");
    static_assert(v > value(std::int64_t(0)), "");
    static_assert(v >= value(std::int64_t(0)), "");

    #ifndef BOOST_NO_CXX17_HDR_OPTIONAL
    static_assert(v.get_std_optional<boost::string_view>(), "");
    #endif
}

BOOST_AUTO_TEST_CASE(float_type)
{
    constexpr value v (3.14f);
    static_assert(!v.is_null(), "");
    static_assert(v.is<float>(), "");
    static_assert(v.is_convertible_to<float>(), "");
    static_assert(v.is_convertible_to<double>(), "");
    static_assert(v.to_variant() == vt(3.14f), "");
    static_assert(v == v, "");
    static_assert(v != value(std::int64_t(0)), "");
    static_assert(v < value(boost::mysql::time(9999)), "");
    static_assert(v <= value(boost::mysql::time(9999)), "");
    static_assert(v > value(std::int64_t(0)), "");
    static_assert(v >= value(std::int64_t(0)), "");

    #ifndef BOOST_NO_CXX17_HDR_OPTIONAL
    static_assert(v.get_std_optional<float>(), "");
    #endif
}

BOOST_AUTO_TEST_CASE(double_type)
{
    constexpr value v (3.14);
    static_assert(!v.is_null(), "");
    static_assert(v.is<double>(), "");
    static_assert(v.is_convertible_to<double>(), "");
    static_assert(v.to_variant() == vt(3.14), "");
    static_assert(v == v, "");
    static_assert(v != value(std::int64_t(0)), "");
    static_assert(v < value(boost::mysql::time(9999)), "");
    static_assert(v <= value(boost::mysql::time(9999)), "");
    static_assert(v > value(std::int64_t(0)), "");
    static_assert(v >= value(std::int64_t(0)), "");

    #ifndef BOOST_NO_CXX17_HDR_OPTIONAL
    static_assert(v.get_std_optional<double>(), "");
    #endif
}

BOOST_AUTO_TEST_CASE(date_type)
{
    constexpr date d (days(1));
    constexpr value v (d);
    static_assert(!v.is_null(), "");
    static_assert(v.is<date>(), "");
    static_assert(v.is_convertible_to<date>(), "");
    static_assert(v.to_variant() == vt(d), "");
    static_assert(v == v, "");
    static_assert(v != value(std::int64_t(0)), "");
    static_assert(v < value(boost::mysql::time(9999)), "");
    static_assert(v <= value(boost::mysql::time(9999)), "");
    static_assert(v > value(std::int64_t(0)), "");
    static_assert(v >= value(std::int64_t(0)), "");

    #ifndef BOOST_NO_CXX17_HDR_OPTIONAL
    static_assert(v.get_std_optional<date>(), "");
    #endif
}

BOOST_AUTO_TEST_CASE(datetime_type)
{
    constexpr datetime d (days(1));
    constexpr value v (d);
    static_assert(!v.is_null(), "");
    static_assert(v.is<datetime>(), "");
    static_assert(v.is_convertible_to<datetime>(), "");
    static_assert(v.to_variant() == vt(d), "");
    static_assert(v == v, "");
    static_assert(v != value(std::int64_t(0)), "");
    static_assert(v < value(boost::mysql::time(9999)), "");
    static_assert(v <= value(boost::mysql::time(9999)), "");
    static_assert(v > value(std::int64_t(0)), "");
    static_assert(v >= value(std::int64_t(0)), "");

    #ifndef BOOST_NO_CXX17_HDR_OPTIONAL
    static_assert(v.get_std_optional<datetime>(), "");
    #endif
}

BOOST_AUTO_TEST_CASE(time_type)
{
    constexpr boost::mysql::time t (1);
    constexpr value v (t);
    static_assert(!v.is_null(), "");
    static_assert(v.is<boost::mysql::time>(), "");
    static_assert(v.is_convertible_to<boost::mysql::time>(), "");
    static_assert(v.to_variant() == vt(t), "");
    static_assert(v == v, "");
    static_assert(v != value(std::int64_t(0)), "");
    static_assert(v < value(boost::mysql::time(9999)), "");
    static_assert(v <= value(boost::mysql::time(9999)), "");
    static_assert(v > value(std::int64_t(0)), "");
    static_assert(v >= value(std::int64_t(0)), "");

    #ifndef BOOST_NO_CXX17_HDR_OPTIONAL
    static_assert(v.get_std_optional<boost::mysql::time>(), "");
    #endif
}

BOOST_AUTO_TEST_SUITE_END()

#endif