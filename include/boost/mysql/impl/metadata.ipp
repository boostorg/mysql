//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_METADATA_IPP
#define BOOST_MYSQL_IMPL_METADATA_IPP

#pragma once

#include <boost/mysql/metadata.hpp>

namespace boost {
namespace mysql {
namespace detail {

inline column_type compute_field_type_string(std::uint32_t flags)
{
    if (flags & column_flags::set)
        return column_type::set;
    else if (flags & column_flags::enum_)
        return column_type::enum_;
    else if (flags & column_flags::binary)
        return column_type::binary;
    else
        return column_type::char_;
}

inline column_type compute_field_type_var_string(std::uint32_t flags)
{
    if (flags & column_flags::binary)
        return column_type::varbinary;
    else
        return column_type::varchar;
}

inline column_type compute_field_type_blob(std::uint32_t flags)
{
    if (flags & column_flags::binary)
        return column_type::blob;
    else
        return column_type::text;
}

inline column_type compute_field_type(protocol_field_type protocol_type, std::uint32_t flags)
{
    switch (protocol_type)
    {
    case protocol_field_type::decimal:
    case protocol_field_type::newdecimal: return column_type::decimal;
    case protocol_field_type::geometry: return column_type::geometry;
    case protocol_field_type::tiny: return column_type::tinyint;
    case protocol_field_type::short_: return column_type::smallint;
    case protocol_field_type::int24: return column_type::mediumint;
    case protocol_field_type::long_: return column_type::int_;
    case protocol_field_type::longlong: return column_type::bigint;
    case protocol_field_type::float_: return column_type::float_;
    case protocol_field_type::double_: return column_type::double_;
    case protocol_field_type::bit: return column_type::bit;
    case protocol_field_type::date: return column_type::date;
    case protocol_field_type::datetime: return column_type::datetime;
    case protocol_field_type::timestamp: return column_type::timestamp;
    case protocol_field_type::time: return column_type::time;
    case protocol_field_type::year: return column_type::year;
    case protocol_field_type::string: return compute_field_type_string(flags);
    case protocol_field_type::var_string: return compute_field_type_var_string(flags);
    case protocol_field_type::blob: return compute_field_type_blob(flags);
    default: return column_type::unknown;
    }
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

inline boost::mysql::column_type boost::mysql::metadata::type() const noexcept
{
    return detail::compute_field_type(type_, flags_);
}

#endif
