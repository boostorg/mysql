//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_FIELD_HPP
#define BOOST_MYSQL_IMPL_FIELD_HPP

#pragma once

#include <boost/mysql/field.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/detail/protocol/date.hpp>
#include <limits>
#include <cstdint>
#include <string>

void boost::mysql::field::from_view(
    const field_view& fv
)
{
    switch (fv.kind())
    {
        case field_kind::null: repr_.emplace<null_t>(null_t()); break;
        case field_kind::int64: repr_.emplace<std::int64_t>(fv.get_int64()); break;
        case field_kind::uint64: repr_.emplace<std::uint64_t>(fv.get_uint64()); break;
        case field_kind::string: repr_.emplace<std::string>(fv.get_string()); break;
        case field_kind::float_: repr_.emplace<float>(fv.get_float()); break;
        case field_kind::double_: repr_.emplace<double>(fv.get_double()); break;
        case field_kind::date: repr_.emplace<date>(fv.get_date()); break;
        case field_kind::datetime: repr_.emplace<datetime>(fv.get_datetime()); break;
        case field_kind::time: repr_.emplace<time>(fv.get_time()); break;
    }
}

inline boost::mysql::field::operator field_view() const noexcept
{
    switch (kind())
    {
        case field_kind::null: return field_view();
        case field_kind::int64: return field_view(get_int64());
        case field_kind::uint64: return field_view(get_uint64());
        case field_kind::string: return field_view(get_string());
        case field_kind::float_: return field_view(get_float());
        case field_kind::double_: return field_view(get_double());
        case field_kind::date: return field_view(get_date());
        case field_kind::datetime: return field_view(get_datetime());
        case field_kind::time: return field_view(get_time());
    }
}

template <typename T>
const T& boost::mysql::field::internal_as() const
{
    const T* res = boost::variant2::get_if<T>(&repr_);
    if (!res)
        throw bad_field_access();
    return *res;
}

template <typename T>
T& boost::mysql::field::internal_as()
{
    T* res = boost::variant2::get_if<T>(&repr_);
    if (!res)
        throw bad_field_access();
    return *res;
}

template <typename T>
const T& boost::mysql::field::internal_get() const noexcept
{
    // TODO: this can be done better
    const T* res = boost::variant2::get_if<T>(&repr_);
    assert(res);
    return *res;
}

template <typename T>
T& boost::mysql::field::internal_get() noexcept
{
    // TODO: this can be done better
    T* res = boost::variant2::get_if<T>(&repr_);
    assert(res);
    return *res;
}

inline std::ostream& boost::mysql::operator<<(
    std::ostream& os,
    const field& value
)
{
    return os << field_view(value);
}

#endif
