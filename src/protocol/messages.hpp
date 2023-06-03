//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PROTOCOL_COMMON_MESSAGES_HPP
#define BOOST_MYSQL_DETAIL_PROTOCOL_COMMON_MESSAGES_HPP

#include <boost/mysql/field_view.hpp>

#include <boost/mysql/detail/auxiliar/static_string.hpp>

#include <cstddef>

#include "protocol/capabilities.hpp"
#include "protocol/constants.hpp"
#include "protocol/protocol_types.hpp"
#include "protocol/serialization.hpp"

namespace boost {
namespace mysql {
namespace detail {

struct frame_header_packet
{
    int3 packet_size;
    std::uint8_t sequence_number;
};

struct ok_packet
{
    // header: int<1>     header     0x00 or 0xFE the OK packet header
    int_lenenc affected_rows;
    int_lenenc last_insert_id;
    std::uint16_t status_flags;  // server_status_flags
    std::uint16_t warnings;
    // CLIENT_SESSION_TRACK: not implemented
    string_lenenc info;
};
inline deserialize_errc deserialize(deserialization_context& ctx, ok_packet& pack) noexcept
{
    auto err = deserialize(ctx, pack.affected_rows, pack.last_insert_id, pack.status_flags, pack.warnings);
    if (err == deserialize_errc::ok && ctx.enough_size(1))  // message is optional, may be omitted
    {
        err = deserialize(ctx, pack.info);
    }
    return err;
}

struct err_packet
{
    // int<1>     header     0xFF ERR packet header
    std::uint16_t error_code;
    string_fixed<1> sql_state_marker;
    string_fixed<5> sql_state;
    string_eof error_message;
};
inline deserialize_errc deserialize(deserialization_context& ctx, err_packet& pack) noexcept
{
    return deserialize(ctx, pack.error_code, pack.sql_state_marker, pack.sql_state, pack.error_message);
}

struct column_definition_packet
{
    string_lenenc catalog;        // always "def"
    string_lenenc schema;         // database
    string_lenenc table;          // virtual table
    string_lenenc org_table;      // physical table
    string_lenenc name;           // virtual column name
    string_lenenc org_name;       // physical column name
    std::uint16_t character_set;  // collation id, somehow named character_set in the protocol docs
    std::uint32_t column_length;  // maximum length of the field
    protocol_field_type type;     // type of the column as defined in enum_field_types
    std::uint16_t flags;          // Flags as defined in Column Definition Flags
    std::uint8_t decimals;        // max shown decimal digits. 0x00 for int/static strings; 0x1f for
                                  // dynamic strings, double, float
};
inline deserialize_errc deserialize(deserialization_context& ctx, column_definition_packet& output) noexcept
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

struct com_query_packet
{
    string_eof query;
    static constexpr std::uint8_t command_id = 3;
};
inline std::size_t get_size(const serialization_context& ctx, com_query_packet pack) noexcept
{
    return get_size(ctx, com_query_packet::command_id, pack.query);
}
inline void serialize(serialization_context& ctx, com_query_packet pack) noexcept
{
    serialize(ctx, com_query_packet::command_id, pack.query);
}

// prepared statement messages
struct com_stmt_prepare_packet
{
    string_eof statement;

    static constexpr std::uint8_t command_id = 0x16;
};
inline std::size_t get_size(const serialization_context& ctx, com_stmt_prepare_packet pack) noexcept
{
    return get_size(ctx, com_stmt_prepare_packet::command_id, pack.statement);
}
inline void serialize(serialization_context& ctx, com_stmt_prepare_packet pack) noexcept
{
    serialize(ctx, com_stmt_prepare_packet::command_id, pack.statement);
}

struct com_stmt_prepare_ok_packet
{
    // std::uint8_t status: must be 0
    std::uint32_t statement_id;
    std::uint16_t num_columns;
    std::uint16_t num_params;
    // std::uint8_t reserved_1: must be 0
    std::uint16_t warning_count;
    // std::uint8_t metadata_follows when CLIENT_OPTIONAL_RESULTSET_METADATA: not implemented
};
inline deserialize_errc deserialize(deserialization_context& ctx, com_stmt_prepare_ok_packet& output) noexcept
{
    std::uint8_t reserved;
    return deserialize(
        ctx,
        output.statement_id,
        output.num_columns,
        output.num_params,
        reserved,
        output.warning_count
    );
}

struct com_stmt_execute_packet_head
{
    // command ID
    std::uint32_t statement_id;
    std::uint8_t flags;
    std::uint32_t iteration_count;
    // if num_params > 0:
    // NULL bitmap
    std::uint8_t new_params_bind_flag;
    // array<com_stmt_execute_param_meta_packet, num_params> meta;
    // array<field_view, num_params> params;
};

struct com_stmt_execute_param_meta_packet
{
    protocol_field_type type;
    std::uint8_t unsigned_flag;
};

// handshake messages
struct handshake_packet
{
    using auth_buffer_type = static_string<8 + 0xff>;

    // int<1>     protocol version     Always 10
    string_null server_version;
    std::uint32_t connection_id;
    auth_buffer_type auth_plugin_data;  // not an actual protocol field, the merge of two fields
    std::uint32_t capability_falgs;     // merge of the two parts - not an actual field
    std::uint8_t character_set;         // default server a_protocol_character_set, only the lower 8-bits
    std::uint16_t status_flags;         // server_status_flags
    string_null auth_plugin_name;
};
inline deserialize_errc deserialize(deserialization_context& ctx, handshake_packet& output) noexcept
{
    constexpr std::uint8_t auth1_length = 8;
    string_fixed<auth1_length> auth_plugin_data_part_1;
    string_fixed<2> capability_flags_low;
    string_fixed<2> capability_flags_high;
    std::uint8_t filler;  // should be 0
    std::uint8_t auth_plugin_data_len = 0;
    string_fixed<10> reserved;

    auto err = deserialize(
        ctx,
        output.server_version,
        output.connection_id,
        auth_plugin_data_part_1,
        filler,
        capability_flags_low,
        output.character_set,
        output.status_flags,
        capability_flags_high
    );
    if (err != deserialize_errc::ok)
        return err;

    // Compose capabilities
    auto capabilities_begin = reinterpret_cast<std::uint8_t*>(&output.capability_falgs);
    memcpy(capabilities_begin, capability_flags_low.data(), 2);
    memcpy(capabilities_begin + 2, capability_flags_high.data(), 2);
    boost::endian::little_to_native_inplace(output.capability_falgs);

    // Check minimum server capabilities to deserialize this frame
    capabilities cap(output.capability_falgs);
    if (!cap.has(CLIENT_PLUGIN_AUTH))
        return deserialize_errc::server_unsupported;

    // Deserialize following fields
    err = deserialize(ctx, auth_plugin_data_len, reserved);
    if (err != deserialize_errc::ok)
        return err;

    // Auth plugin data, second part
    auto auth2_length = static_cast<std::uint8_t>((std::max)(13, auth_plugin_data_len - auth1_length));
    const void* auth2_data = ctx.first();
    if (!ctx.enough_size(auth2_length))
        return deserialize_errc::incomplete_message;
    ctx.advance(auth2_length);

    // Auth plugin name
    err = deserialize(ctx, output.auth_plugin_name);
    if (err != deserialize_errc::ok)
        return err;

    // Compose auth_plugin_data
    output.auth_plugin_data.clear();
    output.auth_plugin_data.append(auth_plugin_data_part_1.data(), auth1_length);
    output.auth_plugin_data.append(auth2_data,
                                   auth2_length - 1);  // discard an extra trailing NULL byte

    return deserialize_errc::ok;
}

struct handshake_response_packet
{
    std::uint32_t client_flag;  // capabilities
    std::uint32_t max_packet_size;
    std::uint8_t character_set;
    // string[23]     filler     filler to the size of the handhshake response packet. All 0s.
    string_null username;
    string_lenenc auth_response;     // we require CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA
    string_null database;            // only to be serialized if CLIENT_CONNECT_WITH_DB
    string_null client_plugin_name;  // we require CLIENT_PLUGIN_AUTH
    // CLIENT_CONNECT_ATTRS: not implemented
};
inline std::size_t get_size(const serialization_context& ctx, const handshake_response_packet& value) noexcept
{
    std::size_t res = get_size(
                          ctx,
                          value.client_flag,
                          value.max_packet_size,
                          value.character_set,
                          value.username,
                          value.auth_response
                      ) +
                      23;  // filler
    if (capabilities(value.client_flag).has(CLIENT_CONNECT_WITH_DB))
    {
        res += get_size(ctx, value.database);
    }
    res += get_size(ctx, value.client_plugin_name);
    return res;
}
inline void serialize(serialization_context& ctx, const handshake_response_packet& value) noexcept
{
    serialize(
        ctx,
        value.client_flag,
        value.max_packet_size,
        value.character_set,
        string_fixed<23>{},
        value.username,
        value.auth_response
    );

    if (capabilities(value.client_flag).has(CLIENT_CONNECT_WITH_DB))
    {
        serialize(ctx, value.database);
    }
    serialize(ctx, value.client_plugin_name);
}

struct ssl_request_packet
{
    std::uint32_t client_flag;
    std::uint32_t max_packet_size;
    std::uint8_t character_set;
    string_fixed<23> filler;
};
inline std::size_t get_size(const serialization_context& ctx, const ssl_request_packet& value) noexcept
{
    return get_size(ctx, value.client_flag, value.max_packet_size, value.character_set, value.filler);
}
inline void serialize(serialization_context& ctx, const ssl_request_packet& value) noexcept
{
    serialize(ctx, value.client_flag, value.max_packet_size, value.character_set, value.filler);
}

struct auth_switch_request_packet
{
    string_null plugin_name;
    string_eof auth_plugin_data;
};
inline deserialize_errc deserialize(deserialization_context& ctx, auth_switch_request_packet& output) noexcept
{
    auto err = deserialize(ctx, output.plugin_name, output.auth_plugin_data);
    auto& auth_data = output.auth_plugin_data.value;
    // Discard an additional NULL at the end of auth data
    if (!auth_data.empty() && auth_data.back() == 0)
    {
        auth_data = auth_data.substr(0, auth_data.size() - 1);
    }
    return err;
}

struct auth_switch_response_packet
{
    string_eof auth_plugin_data;
};
inline std::size_t get_size(
    const serialization_context& ctx,
    const auth_switch_response_packet& value
) noexcept
{
    return get_size(ctx, value.auth_plugin_data);
}
inline void serialize(serialization_context& ctx, const auth_switch_response_packet& value) noexcept
{
    serialize(ctx, value.auth_plugin_data);
}

struct auth_more_data_packet
{
    string_eof auth_plugin_data;
};
inline deserialize_errc deserialize(deserialization_context& ctx, auth_more_data_packet& output) noexcept
{
    return deserialize(ctx, output.auth_plugin_data);
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_COMMON_MESSAGES_HPP_ */
