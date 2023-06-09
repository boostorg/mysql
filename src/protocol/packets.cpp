//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "protocol/packets.hpp"

#include <boost/mysql/column_type.hpp>
#include <boost/mysql/common_server_errc.hpp>
#include <boost/mysql/error_categories.hpp>
#include <boost/mysql/field_kind.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/column_flags.hpp>

#include <boost/endian/conversion.hpp>

#include "error/server_error_to_string.hpp"
#include "protocol/basic_types.hpp"
#include "protocol/binary_serialization.hpp"
#include "protocol/capabilities.hpp"
#include "protocol/constants.hpp"
#include "protocol/null_bitmap_traits.hpp"
#include "protocol/protocol_field_type.hpp"
#include "protocol/serialization.hpp"

using namespace boost::mysql::detail;
using boost::mysql::column_type;
using boost::mysql::field_kind;
using boost::mysql::field_view;
using boost::mysql::string_view;

// ok_view
deserialize_errc boost::mysql::detail::deserialize(deserialization_context& ctx, ok_view& output) noexcept
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

// error packets
deserialize_errc boost::mysql::detail::deserialize(deserialization_context& ctx, err_view& output) noexcept
{
    struct err_packet
    {
        // int<1>     header     0xFF ERR packet header
        std::uint16_t error_code;
        string_fixed<1> sql_state_marker;
        string_fixed<5> sql_state;
        string_eof error_message;
    } pack{};

    auto err = deserialize(ctx, pack.error_code, pack.sql_state_marker, pack.sql_state, pack.error_message);
    if (err != deserialize_errc::ok)
        return err;

    output = err_view{
        pack.error_code,
        pack.error_message.value,
    };
    return deserialize_errc::ok;
}

// coldef_view
static column_type compute_field_type_string(std::uint16_t flags, std::uint16_t collation) noexcept
{
    if (flags & column_flags::set)
        return column_type::set;
    else if (flags & column_flags::enum_)
        return column_type::enum_;
    else if (collation == binary_collation)
        return column_type::binary;
    else
        return column_type::char_;
}

static column_type compute_field_type_var_string(std::uint16_t collation) noexcept
{
    return collation == binary_collation ? column_type::varbinary : column_type::varchar;
}

static column_type compute_field_type_blob(std::uint16_t collation) noexcept
{
    return collation == binary_collation ? column_type::blob : column_type::text;
}

column_type boost::mysql::detail::compute_column_type(
    protocol_field_type protocol_type,
    std::uint16_t flags,
    std::uint16_t collation
) noexcept
{
    // Some protocol_field_types seem to not be sent by the server. We've found instances
    // where some servers, with certain SQL statements, send some of the "apparently not sent"
    // types (e.g. MariaDB was sending medium_blob only if you SELECT TEXT variables - but not with TEXT
    // columns). So we've taken a defensive approach here
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
    case protocol_field_type::json: return column_type::json;
    case protocol_field_type::enum_: return column_type::enum_;  // in theory not set
    case protocol_field_type::set: return column_type::set;      // in theory not set
    case protocol_field_type::string: return compute_field_type_string(flags, collation);
    case protocol_field_type::varchar:  // in theory not sent
    case protocol_field_type::var_string: return compute_field_type_var_string(collation);
    case protocol_field_type::tiny_blob:    // in theory not sent
    case protocol_field_type::medium_blob:  // in theory not sent
    case protocol_field_type::long_blob:    // in theory not sent
    case protocol_field_type::blob: return compute_field_type_blob(collation);
    default: return column_type::unknown;
    }
}

deserialize_errc boost::mysql::detail::deserialize(deserialization_context& ctx, coldef_view& output) noexcept
{
    struct column_definition_packet
    {
        string_lenenc catalog;    // always "def"
        string_lenenc schema;     // database
        string_lenenc table;      // virtual table
        string_lenenc org_table;  // physical table
        string_lenenc name;       // virtual column name
        string_lenenc org_name;   // physical column name
        string_lenenc fixed_fields;
    } pack{};

    // pack.fixed_fields itself is a structure like this.
    // The proto allows for extensibility here - adding fields just increasing fixed_fields.length
    struct fixed_fields_pack
    {
        std::uint16_t character_set;  // collation id, somehow named character_set in the protocol docs
        std::uint32_t column_length;  // maximum length of the field
        protocol_field_type type;     // type of the column as defined in enum_field_types
        std::uint16_t flags;          // Flags as defined in Column Definition Flags
        std::uint8_t decimals;        // max shown decimal digits. 0x00 for int/static strings; 0x1f for
                                      // dynamic strings, double, float
    } fixed_fields{};

    // Deserialize the main structure
    auto err = deserialize(
        ctx,
        pack.catalog,
        pack.schema,
        pack.table,
        pack.org_table,
        pack.name,
        pack.org_name,
        pack.fixed_fields
    );
    if (err != deserialize_errc::ok)
        return err;

    // Deserialize the fixed_fields structure.
    // Intentionally not checking for extra bytes here, since there may be unknown fields that should just get
    // ignored
    deserialization_context subctx(pack.fixed_fields.value.data(), pack.fixed_fields.value.size());
    err = deserialize(
        subctx,
        fixed_fields.character_set,
        fixed_fields.column_length,
        fixed_fields.type,
        fixed_fields.flags,
        fixed_fields.decimals
    );
    if (err != deserialize_errc::ok)
        return err;

    // Compose output
    output = coldef_view{
        pack.schema.value,
        pack.table.value,
        pack.org_table.value,
        pack.name.value,
        pack.org_name.value,
        fixed_fields.character_set,
        fixed_fields.column_length,
        compute_column_type(fixed_fields.type, fixed_fields.flags, fixed_fields.character_set),
        fixed_fields.flags,
        fixed_fields.decimals,
    };
    return deserialize_errc::ok;
}

deserialize_errc boost::mysql::detail::deserialize(
    deserialization_context& ctx,
    prepare_stmt_response& output
) noexcept
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
static protocol_field_type get_protocol_field_type(field_view input) noexcept
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

std::size_t boost::mysql::detail::get_size(execute_stmt_command value) noexcept
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

void boost::mysql::detail::serialize(serialization_context& ctx, execute_stmt_command value) noexcept
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

// server_hello
static capabilities compose_capabilities(string_fixed<2> low, string_fixed<2> high) noexcept
{
    std::uint32_t res = 0;
    auto capabilities_begin = reinterpret_cast<std::uint8_t*>(&res);
    memcpy(capabilities_begin, low.value.data(), 2);
    memcpy(capabilities_begin + 2, high.value.data(), 2);
    return capabilities(boost::endian::little_to_native(res));
}

static db_flavor parse_db_version(string_view version_string) noexcept
{
    return version_string.find("MariaDB") != string_view::npos ? db_flavor::mariadb : db_flavor::mysql;
}

deserialize_errc boost::mysql::detail::deserialize(
    deserialization_context& ctx,
    server_hello& output
) noexcept
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

// login_request
static std::uint8_t get_collation_first_byte(std::uint32_t collation_id) noexcept
{
    return static_cast<std::uint8_t>(collation_id % 0xff);
}

namespace {

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

}  // namespace

static login_request_packet to_packet(const login_request& req) noexcept
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

std::size_t boost::mysql::detail::get_size(const login_request& value) noexcept
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

void boost::mysql::detail::serialize(serialization_context& ctx, const login_request& value) noexcept
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

// ssl_request
std::size_t boost::mysql::detail::get_size(ssl_request) noexcept { return 4 + 4 + 1 + 23; }
void boost::mysql::detail::serialize(serialization_context& ctx, ssl_request value) noexcept
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

// auth_switch
deserialize_errc boost::mysql::detail::deserialize(deserialization_context& ctx, auth_switch& output) noexcept
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
        to_span(auth_data),
    };
    return deserialize_errc::ok;
}
