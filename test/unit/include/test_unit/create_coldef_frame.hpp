//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_CREATE_COLDEF_FRAME_HPP
#define BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_CREATE_COLDEF_FRAME_HPP

#include <boost/mysql/detail/coldef_view.hpp>

#include <boost/mysql/impl/internal/protocol/impl/protocol_field_type.hpp>
#include <boost/mysql/impl/internal/protocol/impl/protocol_types.hpp>
#include <boost/mysql/impl/internal/protocol/impl/serialization_context.hpp>

#include <boost/assert.hpp>

#include "test_unit/create_frame.hpp"

namespace boost {
namespace mysql {
namespace test {

inline std::vector<std::uint8_t> create_coldef_body(const detail::coldef_view& pack)
{
    auto to_protocol_type = [](column_type t) {
        // Note: we perform an approximate mapping, good enough for unit tests.
        // The actual mapping is not one to one and depends on flags
        using detail::protocol_field_type;
        switch (t)
        {
        case column_type::tinyint: return protocol_field_type::tiny;
        case column_type::smallint: return protocol_field_type::short_;
        case column_type::mediumint: return protocol_field_type::int24;
        case column_type::int_: return protocol_field_type::long_;
        case column_type::bigint: return protocol_field_type::longlong;
        case column_type::float_: return protocol_field_type::float_;
        case column_type::double_: return protocol_field_type::double_;
        case column_type::decimal: return protocol_field_type::newdecimal;
        case column_type::bit: return protocol_field_type::bit;
        case column_type::year: return protocol_field_type::year;
        case column_type::time: return protocol_field_type::time;
        case column_type::date: return protocol_field_type::date;
        case column_type::datetime: return protocol_field_type::datetime;
        case column_type::timestamp: return protocol_field_type::timestamp;
        case column_type::char_: return protocol_field_type::string;
        case column_type::varchar: return protocol_field_type::var_string;
        case column_type::binary: return protocol_field_type::string;
        case column_type::varbinary: return protocol_field_type::var_string;
        case column_type::text: return protocol_field_type::blob;
        case column_type::blob: return protocol_field_type::blob;
        case column_type::enum_: return protocol_field_type::enum_;
        case column_type::set: return protocol_field_type::set;
        case column_type::json: return protocol_field_type::json;
        case column_type::geometry: return protocol_field_type::geometry;
        default: BOOST_ASSERT(false); return protocol_field_type::var_string;
        }
    };

    std::vector<std::uint8_t> buff;
    detail::serialization_context ctx(buff, detail::disable_framing);
    ctx.serialize(
        detail::string_lenenc{"def"},
        detail::string_lenenc{pack.database},
        detail::string_lenenc{pack.table},
        detail::string_lenenc{pack.org_table},
        detail::string_lenenc{pack.name},
        detail::string_lenenc{pack.org_name},
        detail::int_lenenc{0x0c},  // length of fixed fields
        detail::int2{pack.collation_id},
        detail::int4{pack.column_length},
        detail::int1{static_cast<std::uint8_t>(to_protocol_type(pack.type))},
        detail::int2{pack.flags},
        detail::int1{pack.decimals},
        detail::int2{0}  // padding
    );
    return buff;
}

inline std::vector<std::uint8_t> create_coldef_frame(std::uint8_t seqnum, const detail::coldef_view& coldef)
{
    return create_frame(seqnum, create_coldef_body(coldef));
}

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
