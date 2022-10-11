//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_FIELD_HPP
#define BOOST_MYSQL_IMPL_FIELD_HPP

#pragma once

#include <boost/mysql/bad_field_access.hpp>
#include <boost/mysql/detail/protocol/date.hpp>
#include <boost/mysql/field.hpp>

#include <cstdint>
#include <string>

void boost::mysql::field::from_view(const field_view& fv)
{
    switch (fv.kind())
    {
    case field_kind::null: repr_.data.emplace<detail::field_impl::null_t>(); break;
    case field_kind::int64: repr_.data.emplace<std::int64_t>(fv.get_int64()); break;
    case field_kind::uint64: repr_.data.emplace<std::uint64_t>(fv.get_uint64()); break;
    case field_kind::string: repr_.data.emplace<std::string>(fv.get_string()); break;
    case field_kind::float_: repr_.data.emplace<float>(fv.get_float()); break;
    case field_kind::double_: repr_.data.emplace<double>(fv.get_double()); break;
    case field_kind::date: repr_.data.emplace<date>(fv.get_date()); break;
    case field_kind::datetime: repr_.data.emplace<datetime>(fv.get_datetime()); break;
    case field_kind::time: repr_.data.emplace<time>(fv.get_time()); break;
    }
}

inline std::ostream& boost::mysql::operator<<(std::ostream& os, const field& value)
{
    return os << field_view(value);
}

#endif
