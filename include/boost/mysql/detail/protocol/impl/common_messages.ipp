//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_COMMON_MESSAGES_IPP
#define BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_COMMON_MESSAGES_IPP

#pragma once

#include <boost/mysql/detail/protocol/common_messages.hpp>


inline boost::mysql::errc
boost::mysql::detail::serialization_traits<
    boost::mysql::detail::ok_packet,
    boost::mysql::detail::serialization_tag::struct_with_fields
>::deserialize_(
    deserialization_context& ctx,
    ok_packet& output
) noexcept
{
    {
        auto err = deserialize(
            ctx,
            output.affected_rows,
            output.last_insert_id,
            output.status_flags,
            output.warnings
        );
        if (err == errc::ok && ctx.enough_size(1)) // message is optional, may be omitted
        {
            err = deserialize(ctx, output.info);
        }
        return err;
    }
}

inline boost::mysql::errc
boost::mysql::detail::serialization_traits<
    boost::mysql::detail::column_definition_packet,
    boost::mysql::detail::serialization_tag::struct_with_fields
>::deserialize_(
    deserialization_context& ctx,
    column_definition_packet& output
) noexcept
{
    int_lenenc length_of_fixed_fields;
    std::uint16_t final_padding = 0;
    return deserialize(
        ctx,
        output.catalog,
        output.schema,
        output.table,
        output.org_table,
        output.name,
        output.org_name,
        length_of_fixed_fields,
        output.character_set,
        output.column_length,
        output.type,
        output.flags,
        output.decimals,
        final_padding
    );
}

inline boost::mysql::error_code boost::mysql::detail::process_error_packet(
    deserialization_context& ctx,
    error_info& info
)
{
    err_packet error_packet {};
    auto code = deserialize_message(ctx, error_packet);
    if (code)
        return code;
    info.set_message(error_packet.error_message.value.to_string());
    return make_error_code(static_cast<errc>(error_packet.error_code));
}


#endif /* INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_COMMON_MESSAGES_IPP_ */
