//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_FIELD_VIEW_HPP
#define BOOST_MYSQL_IMPL_FIELD_VIEW_HPP

#pragma once

#include <boost/mysql/field_view.hpp>
#include <boost/mysql/detail/protocol/date.hpp>
#include <limits>

namespace boost {
namespace mysql {
namespace detail {

struct print_visitor
{
    std::ostream& os;

    print_visitor(std::ostream& os): os(os) {};

    template <class T>
    void operator()(const T& value) const { os << value; }

    void operator()(const date& value) const
    {
        assert(value >= min_date && value <= max_date);
        auto ymd = days_to_ymd(value.time_since_epoch().count());
        char buffer [32] {};
        snprintf(buffer, sizeof(buffer), "%04d-%02u-%02u", ymd.years, ymd.month, ymd.day);
        os << buffer;
    }
    void operator()(const datetime& value) const
    {
        using namespace std::chrono;
        date date_part = time_point_cast<days>(value);
        if (date_part > value)
            date_part -= days(1);
        auto tod = value - date_part;
        (*this)(date_part); // date part
        os << ' '; // separator
        (*this)(duration_cast<time>(tod)); // time of day part
    }
    void operator()(const time& value) const
    {
        using namespace std::chrono;
        assert(value >= min_time && value <= max_time);
        char buffer [64] {};
        const char* sign = value < microseconds(0) ? "-" : "";
        auto num_micros = value % seconds(1);
        auto num_secs = duration_cast<seconds>(value % minutes(1) - num_micros);
        auto num_mins = duration_cast<minutes>(value % hours(1) - num_secs);
        auto num_hours = duration_cast<hours>(value - num_mins);

        snprintf(buffer, sizeof(buffer), "%s%02d:%02u:%02u.%06u",
            sign,
            static_cast<int>(std::abs(num_hours.count())),
            static_cast<unsigned>(std::abs(num_mins.count())),
            static_cast<unsigned>(std::abs(num_secs.count())),
            static_cast<unsigned>(std::abs(num_micros.count()))
        );

        os << buffer;
    }
    void operator()(null_t) const { os << "<NULL>"; }
};


} // detail
} // mysql
} // boost

template <typename T>
const T& boost::mysql::field_view::internal_as() const
{
    const T* res = boost::variant2::get_if<T>(&repr_);
    if (!res)
        throw bad_field_access();
    return *res;
}

template <typename T>
const T& boost::mysql::field_view::internal_get() const noexcept
{
    const T* res = boost::variant2::get_if<T>(&repr_);
    assert(res);
    return *res;
}

BOOST_CXX14_CONSTEXPR bool boost::mysql::field_view::operator==(
    const field_view& rhs
) const noexcept
{
    if (kind() == field_kind::int64 && rhs.kind() == field_kind::uint64)
    {
        std::int64_t this_val = get_int64();
        if (this_val < 0)
            return false;
        return static_cast<std::uint64_t>(this_val) == rhs.get_uint64();
    }
    else if (kind() == field_kind::uint64 && rhs.kind() == field_kind::int64)
    {
        std::int64_t rhs_val = rhs.get_int64();
        if (rhs_val < 0)
            return false;
        return static_cast<std::uint64_t>(rhs_val) == get_uint64();
    }
    else
    {
        return repr_ == rhs.repr_;
    }
}


inline std::ostream& boost::mysql::operator<<(
    std::ostream& os,
    const field_view& value
)
{
    boost::variant2::visit(detail::print_visitor(os), value.repr_);
    return os;
}

template <class... Types>
BOOST_CXX14_CONSTEXPR std::array<boost::mysql::field_view, sizeof...(Types)>
boost::mysql::make_field_views(
    Types&&... args
)
{
    return std::array<field_view, sizeof...(Types)>{{field_view(std::forward<Types>(args))...}};
}


#endif
