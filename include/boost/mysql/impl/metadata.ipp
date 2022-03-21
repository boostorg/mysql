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

inline field_type compute_field_type_string(
    std::uint32_t flags
)
{
    if (flags & column_flags::set)
        return field_type::set;
    else if (flags & column_flags::enum_)
        return field_type::enum_;
    else if (flags & column_flags::binary)
        return field_type::binary;
    else
        return field_type::char_;
}

inline field_type compute_field_type_var_string(
    std::uint32_t flags
)
{
    if (flags & column_flags::binary)
        return field_type::varbinary;
    else
        return field_type::varchar;
}

inline field_type compute_field_type_blob(
    std::uint32_t flags
)
{
    if (flags & column_flags::binary)
        return field_type::blob;
    else
        return field_type::text;
}

inline field_type compute_field_type(
    protocol_field_type protocol_type,
    std::uint32_t flags
)
{
    switch (protocol_type)
    {
    case protocol_field_type::decimal:
    case protocol_field_type::newdecimal:
        return field_type::decimal;
    case protocol_field_type::geometry: return field_type::geometry;
    case protocol_field_type::tiny: return field_type::tinyint;
    case protocol_field_type::short_: return field_type::smallint;
    case protocol_field_type::int24: return field_type::mediumint;
    case protocol_field_type::long_: return field_type::int_;
    case protocol_field_type::longlong: return field_type::bigint;
    case protocol_field_type::float_: return field_type::float_;
    case protocol_field_type::double_: return field_type::double_;
    case protocol_field_type::bit: return field_type::bit;
    case protocol_field_type::date: return field_type::date;
    case protocol_field_type::datetime: return field_type::datetime;
    case protocol_field_type::timestamp: return field_type::timestamp;
    case protocol_field_type::time: return field_type::time;
    case protocol_field_type::year: return field_type::year;
    case protocol_field_type::string: return compute_field_type_string(flags);
    case protocol_field_type::var_string: return compute_field_type_var_string(flags);
    case protocol_field_type::blob: return compute_field_type_blob(flags);
    default: return field_type::unknown;
    }
}

} // detail
} // mysql
} // boost

inline boost::mysql::field_type boost::mysql::field_metadata::type() const noexcept
{
    if (field_type_ == field_type::_not_computed)
    {
        field_type_ = detail::compute_field_type(msg_.type, msg_.flags);
        assert(field_type_ != field_type::_not_computed);
    }
    return field_type_;
}

#endif
