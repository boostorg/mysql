//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_SRC_PROTOCOL_MESSAGES_HPP
#define BOOST_MYSQL_SRC_PROTOCOL_MESSAGES_HPP

#include <boost/mysql/field_view.hpp>

#include <boost/endian/conversion.hpp>

#include <cstddef>

#include "protocol/basic_types.hpp"
#include "protocol/binary_serialization.hpp"
#include "protocol/capabilities.hpp"
#include "protocol/constants.hpp"
#include "protocol/null_bitmap_traits.hpp"
#include "protocol/protocol.hpp"
#include "protocol/protocol_field_type.hpp"
#include "protocol/serialization.hpp"
#include "protocol/static_string.hpp"

namespace boost {
namespace mysql {
namespace detail {

struct frame_header_packet
{
    int3 packet_size;
    std::uint8_t sequence_number;
};

// ok_view
inline deserialize_errc deserialize(deserialization_context& ctx, ok_view& output) noexcept
{
    struct ok_packet
    {
        // header: int<1>     header     0x00 or 0xFE the OK packet header
        int_lenenc affected_rows;
        int_lenenc last_insert_id;
        std::uint16_t status_flags;  // server_status_flags
        std::uint16_t warnings;
        // CLIENT_SESSION_TRACK: not implemented
        string_lenenc info;
    } pack{};

    auto err = deserialize(ctx, pack.affected_rows, pack.last_insert_id, pack.status_flags, pack.warnings);
    if (err != deserialize_errc::ok)
        return err;

    if (ctx.enough_size(1))  // message is optional, may be omitted
    {
        err = deserialize(ctx, pack.info);
        if (err != deserialize_errc::ok)
            return err;
    }

    output = {
        pack.affected_rows.value,
        pack.last_insert_id.value,
        pack.status_flags,
        pack.warnings,
        pack.info.value,
    };
    return deserialize_errc::ok;
}

// Error packet. This is not exposed in the protocol interface
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

// Column definition
inline deserialize_errc deserialize(deserialization_context& ctx, coldef_view& output) noexcept
{
    struct column_definition_packet
    {
        string_lenenc catalog;    // always "def"
        string_lenenc schema;     // database
        string_lenenc table;      // virtual table
        string_lenenc org_table;  // physical table
        string_lenenc name;       // virtual column name
        string_lenenc org_name;   // physical column name
        int_lenenc length_of_fixed_fields;
        std::uint16_t character_set;  // collation id, somehow named character_set in the protocol docs
        std::uint32_t column_length;  // maximum length of the field
        protocol_field_type type;     // type of the column as defined in enum_field_types
        std::uint16_t flags;          // Flags as defined in Column Definition Flags
        std::uint8_t decimals;        // max shown decimal digits. 0x00 for int/static strings; 0x1f for
                                      // dynamic strings, double, float
        std::uint16_t final_padding;
    } pack{};

    auto err = deserialize(
        ctx,
        pack.catalog,
        pack.schema,
        pack.table,
        pack.org_table,
        pack.name,
        pack.org_name,
        pack.length_of_fixed_fields,
        pack.character_set,
        pack.column_length,
        pack.type,
        pack.flags,
        pack.decimals,
        pack.final_padding
    );
    // TODO: this should really check length_of_fixed_fields
    if (err != deserialize_errc::ok)
        return err;

    output = coldef_view{
        pack.schema.value,
        pack.table.value,
        pack.org_table.value,
        pack.name.value,
        pack.org_name.value,
        pack.character_set,
        pack.column_length,
        compute_field_type(pack.type, pack.flags, pack.character_set),
        pack.flags,
        pack.decimals,
    };

    return deserialize_errc::ok;
}

// Quit
constexpr std::size_t get_size(quit_command) noexcept { return 1; }
inline void serialize(serialization_context& ctx, quit_command) noexcept
{
    constexpr std::uint8_t command_id = 0x01;
    serialize(ctx, command_id);
}

// Ping
constexpr std::size_t get_size(ping_command) noexcept { return 1; }
inline void serialize(serialization_context& ctx, ping_command) noexcept
{
    constexpr std::uint8_t command_id = 0x0e;
    serialize(ctx, command_id);
}

// Query
inline std::size_t get_size(query_command value) noexcept
{
    return get_size(string_eof{value.query}) + 1;  // command ID
}
inline void serialize(serialization_context& ctx, query_command value) noexcept
{
    constexpr std::uint8_t command_id = 3;
    serialize(ctx, command_id, string_eof(value.query));
}

// Prepare statement
inline std::size_t get_size(prepare_stmt_command value) noexcept
{
    return get_size(string_eof{value.stmt}) + 1;  // command ID
}
inline void serialize(serialization_context& ctx, prepare_stmt_command pack) noexcept
{
    constexpr std::uint8_t command_id = 0x16;
    serialize(ctx, command_id, string_eof{pack.stmt});
}

inline deserialize_errc deserialize(deserialization_context& ctx, prepare_stmt_response& output) noexcept
{
    struct com_stmt_prepare_ok_packet
    {
        // std::uint8_t status: must be 0
        std::uint32_t statement_id;
        std::uint16_t num_columns;
        std::uint16_t num_params;
        std::uint8_t reserved_1;  // must be 0
        std::uint16_t warning_count;
        // std::uint8_t metadata_follows when CLIENT_OPTIONAL_RESULTSET_METADATA: not implemented
    } pack{};

    auto err = deserialize(
        ctx,
        pack.statement_id,
        pack.num_columns,
        pack.num_params,
        pack.reserved_1,
        pack.warning_count
    );
    if (err != deserialize_errc::ok)
        return err;

    output = prepare_stmt_response{pack.statement_id, pack.num_columns, pack.num_params};
    return deserialize_errc::ok;
}

// Execute statement. The wire layout is as follows:
//  command ID
//  std::uint32_t statement_id;
//  std::uint8_t flags;
//  std::uint32_t iteration_count;
//  if num_params > 0:
//      NULL bitmap
//      std::uint8_t new_params_bind_flag;
//      array<meta_packet, num_params> meta;
//          protocol_field_type type;
//          std::uint8_t unsigned_flag;
//      array<field_view, num_params> params;

// Maps from an actual value to a protocol_field_type. Only value's type is used
inline protocol_field_type get_protocol_field_type(field_view input) noexcept
{
    switch (input.kind())
    {
    case field_kind::null: return protocol_field_type::null;
    case field_kind::int64: return protocol_field_type::longlong;
    case field_kind::uint64: return protocol_field_type::longlong;
    case field_kind::string: return protocol_field_type::string;
    case field_kind::blob: return protocol_field_type::blob;
    case field_kind::float_: return protocol_field_type::float_;
    case field_kind::double_: return protocol_field_type::double_;
    case field_kind::date: return protocol_field_type::date;
    case field_kind::datetime: return protocol_field_type::datetime;
    case field_kind::time: return protocol_field_type::time;
    default: BOOST_ASSERT(false); return protocol_field_type::null;
    }
}

inline std::size_t get_size(execute_stmt_command value) noexcept
{
    constexpr std::size_t param_meta_packet_size = 2;           // type + unsigned flag
    constexpr std::size_t stmt_execute_packet_head_size = 1     // command ID
                                                          + 4   // statement_id
                                                          + 1   // flags
                                                          + 4;  // iteration_count
    std::size_t res = stmt_execute_packet_head_size;
    auto num_params = value.params.size();
    if (num_params > 0u)
    {
        res += null_bitmap_traits(stmt_execute_null_bitmap_offset, num_params).byte_count();
        res += 1;  // new_params_bind_flag
        res += param_meta_packet_size * num_params;
        for (field_view param : value.params)
        {
            res += get_size(param);
        }
    }

    return res;
}

inline void serialize(serialization_context& ctx, execute_stmt_command value) noexcept
{
    constexpr std::uint8_t command_id = 0x17;

    std::uint32_t statement_id = value.statement_id;
    std::uint8_t flags = 0;
    std::uint32_t iteration_count = 1;
    std::uint8_t new_params_bind_flag = 1;

    serialize(ctx, command_id, statement_id, flags, iteration_count);

    // Number of parameters
    auto num_params = value.params.size();

    if (num_params > 0)
    {
        // NULL bitmap
        null_bitmap_traits traits(stmt_execute_null_bitmap_offset, num_params);
        std::memset(ctx.first(), 0, traits.byte_count());  // Initialize to zeroes
        for (std::size_t i = 0; i < num_params; ++i)
        {
            if (value.params[i].is_null())
            {
                traits.set_null(ctx.first(), i);
            }
        }
        ctx.advance(traits.byte_count());

        // new parameters bind flag
        serialize(ctx, new_params_bind_flag);

        // value metadata
        for (field_view param : value.params)
        {
            protocol_field_type type = get_protocol_field_type(param);
            std::uint8_t unsigned_flag = param.is_uint64() ? std::uint8_t(0x80) : std::uint8_t(0);
            serialize(ctx, type, unsigned_flag);
        }

        // actual values
        for (field_view param : value.params)
        {
            serialize(ctx, param);
        }
    }
}

// Close statement
constexpr std::size_t get_size(close_stmt_command) noexcept { return 5; }
inline void serialize(serialization_context& ctx, close_stmt_command value) noexcept
{
    constexpr std::uint8_t command_id = 0x19;
    serialize(ctx, command_id, value.statement_id);
}

// handshake messages
inline capabilities compose_capabilities(string_fixed<2> low, string_fixed<2> high) noexcept
{
    std::uint32_t res = 0;
    auto capabilities_begin = reinterpret_cast<std::uint8_t*>(&res);
    memcpy(capabilities_begin, low.value.data(), 2);
    memcpy(capabilities_begin + 2, high.value.data(), 2);
    return capabilities(endian::little_to_native(res));
}

inline db_flavor parse_db_version(string_view version_string) noexcept
{
    return version_string.find("MariaDB") != string_view::npos ? db_flavor::mariadb : db_flavor::mysql;
}

inline deserialize_errc deserialize(deserialization_context& ctx, server_hello& output) noexcept
{
    static constexpr std::uint8_t auth1_length = 8;

    struct server_hello_packet
    {
        // int<1>     protocol version     Always 10
        string_null server_version;
        std::uint32_t connection_id;
        string_fixed<auth1_length> auth_plugin_data_part_1;
        std::uint8_t filler;  // should be 0
        string_fixed<2> capability_flags_low;
        std::uint8_t character_set;  // default server a_protocol_character_set, only the lower 8-bits
        std::uint16_t status_flags;  // server_status_flags
        string_fixed<2> capability_flags_high;
        std::uint8_t auth_plugin_data_len;
        string_fixed<10> reserved;
        // auth plugin data, 2nd part. This has a weird representation that doesn't fit any defined type
        string_null auth_plugin_name;
    } pack{};

    auto err = deserialize(
        ctx,
        pack.server_version,
        pack.connection_id,
        pack.auth_plugin_data_part_1,
        pack.filler,
        pack.capability_flags_low,
        pack.character_set,
        pack.status_flags,
        pack.capability_flags_high
    );
    if (err != deserialize_errc::ok)
        return err;

    // Compose capabilities
    auto cap = compose_capabilities(pack.capability_flags_low, pack.capability_flags_high);

    // Check minimum server capabilities to deserialize this frame
    if (!cap.has(CLIENT_PLUGIN_AUTH))
        return deserialize_errc::server_unsupported;

    // Deserialize following fields
    err = deserialize(ctx, pack.auth_plugin_data_len, pack.reserved);
    if (err != deserialize_errc::ok)
        return err;

    // Auth plugin data, second part
    auto auth2_length = static_cast<std::uint8_t>((std::max)(13, pack.auth_plugin_data_len - auth1_length));
    const void* auth2_data = ctx.first();
    if (!ctx.enough_size(auth2_length))
        return deserialize_errc::incomplete_message;
    ctx.advance(auth2_length);

    // Auth plugin name
    err = deserialize(ctx, pack.auth_plugin_name);
    if (err != deserialize_errc::ok)
        return err;

    // Compose output
    output.server = parse_db_version(pack.server_version.value);
    output.server_capabilities = cap;
    output.auth_plugin_name = pack.auth_plugin_name.value;

    // Compose auth_plugin_data
    output.auth_plugin_data.clear();
    output.auth_plugin_data.append(pack.auth_plugin_data_part_1.value.data(), auth1_length);
    output.auth_plugin_data.append(auth2_data,
                                   auth2_length - 1);  // discard an extra trailing NULL byte

    return deserialize_errc::ok;
}

inline std::uint8_t get_collation_first_byte(std::uint32_t collation_id) noexcept
{
    return static_cast<std::uint8_t>(collation_id % 0xff);
}

inline string_view to_string(span<const std::uint8_t> value) noexcept
{
    return string_view(reinterpret_cast<const char*>(value.data()), value.size());
}

struct login_request_packet
{
    std::uint32_t client_flag;  // capabilities
    std::uint32_t max_packet_size;
    std::uint8_t character_set;  // collation ID first byte
    string_fixed<23> filler;     //     All 0s.
    string_null username;
    string_lenenc auth_response;     // we require CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA
    string_null database;            // only to be serialized if CLIENT_CONNECT_WITH_DB
    string_null client_plugin_name;  // we require CLIENT_PLUGIN_AUTH
    // CLIENT_CONNECT_ATTRS: not implemented
};

inline login_request_packet to_packet(const login_request& req) noexcept
{
    return {
        req.negotiated_capabilities.get(),
        req.max_packet_size,
        get_collation_first_byte(req.collation_id),
        {},
        string_null{req.username},
        string_lenenc{to_string(req.auth_response)},
        string_null{req.database},
        string_null{req.auth_plugin_name},
    };
}

inline std::size_t get_size(const login_request& value) noexcept
{
    auto pack = to_packet(value);
    return get_size(
               pack.client_flag,
               pack.max_packet_size,
               pack.character_set,
               pack.filler,
               pack.username,
               pack.auth_response
           ) +
           (value.negotiated_capabilities.has(CLIENT_CONNECT_WITH_DB) ? get_size(string_null{value.database})
                                                                      : 0) +
           get_size(pack.client_plugin_name);
}
inline void serialize(serialization_context& ctx, const login_request& value) noexcept
{
    auto pack = to_packet(value);

    serialize(
        ctx,
        pack.client_flag,
        pack.max_packet_size,
        pack.character_set,
        pack.filler,
        pack.username,
        string_lenenc(to_string(value.auth_response))
    );
    if (value.negotiated_capabilities.has(CLIENT_CONNECT_WITH_DB))
    {
        serialize(ctx, string_null{value.database});
    }
    serialize(ctx, string_null{value.auth_plugin_name});
}

constexpr std::size_t get_size(ssl_request) noexcept { return 4 + 4 + 1 + 23; }
inline void serialize(serialization_context& ctx, ssl_request value) noexcept
{
    struct ssl_request_packet
    {
        std::uint32_t client_flag;
        std::uint32_t max_packet_size;
        std::uint8_t character_set;
        string_fixed<23> filler;
    } pack{
        value.negotiated_capabilities.get(),
        value.max_packet_size,
        get_collation_first_byte(value.collation_id),
        {},
    };

    serialize(ctx, pack.client_flag, pack.max_packet_size, pack.character_set, pack.filler);
}

inline deserialize_errc deserialize(deserialization_context& ctx, auth_switch& output) noexcept
{
    struct auth_switch_request_packet
    {
        string_null plugin_name;
        string_eof auth_plugin_data;
    } pack{};

    auto err = deserialize(ctx, pack.plugin_name, pack.auth_plugin_data);
    if (err != deserialize_errc::ok)
        return err;

    // Discard an additional NULL at the end of auth data
    string_view auth_data = pack.auth_plugin_data.value;
    if (!auth_data.empty() && auth_data.back() == 0)
    {
        auth_data = auth_data.substr(0, auth_data.size() - 1);
    }

    output = {
        pack.plugin_name.value,
        span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(auth_data.data()), auth_data.size()),
    };
    return deserialize_errc::ok;
}

inline std::size_t get_size(auth_switch_response value) noexcept
{
    return get_size(string_eof{to_string(value.auth_plugin_data)});
}
inline void serialize(serialization_context& ctx, auth_switch_response value) noexcept
{
    serialize(ctx, string_eof{to_string(value.auth_plugin_data)});
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
