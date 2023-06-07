//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "protocol/protocol.hpp"

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/column_type.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/field_kind.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/metadata_collection_view.hpp>
#include <boost/mysql/statement.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/column_flags.hpp>

#include <boost/asio/buffer.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/core/span.hpp>

#include <cstddef>

#include "make_string_view.hpp"
#include "protocol/binary_serialization.hpp"
#include "protocol/capabilities.hpp"
#include "protocol/constants.hpp"
#include "protocol/deserialize_binary_field.hpp"
#include "protocol/deserialize_text_field.hpp"
#include "protocol/messages.hpp"
#include "protocol/null_bitmap_traits.hpp"
#include "protocol/process_error_packet.hpp"
#include "protocol/protocol_types.hpp"
#include "protocol/serialization.hpp"

using boost::mysql::client_errc;
using boost::mysql::column_type;
using boost::mysql::error_code;
using boost::mysql::field_kind;
using boost::mysql::field_view;
using boost::mysql::metadata_collection_view;
using boost::mysql::string_view;
using namespace boost::mysql::detail;

// Constants
static constexpr std::uint8_t handshake_protocol_version_9 = 9;
static constexpr std::uint8_t handshake_protocol_version_10 = 10;
static constexpr std::uint8_t error_packet_header = 0xff;
static constexpr std::uint8_t ok_packet_header = 0x00;
static constexpr std::uint8_t eof_packet_header = 0xfe;
static constexpr std::uint8_t auth_switch_request_header = 0xfe;
static constexpr std::uint8_t auth_more_data_header = 0x01;
static constexpr string_view fast_auth_complete_challenge = make_string_view("\3");

void boost::mysql::detail::serialize_frame_header(
    frame_header msg,
    span<std::uint8_t, frame_header_size> buffer
) noexcept
{
    BOOST_ASSERT(msg.size <= 0xffffff);                        // range check
    serialization_context ctx(capabilities(), buffer.data());  // unaffected by capabilities
    frame_header_packet pack{int3{msg.size}, msg.sequence_number};
    serialize(ctx, pack.packet_size, pack.sequence_number);
}

frame_header boost::mysql::detail::deserialize_frame_header(span<const std::uint8_t, frame_header_size> buffer
) noexcept
{
    frame_header_packet pack{};
    deserialization_context ctx(buffer.data(), buffer.size(), capabilities());  // Unaffected by capacilities
    auto err = deserialize(ctx, pack.packet_size, pack.sequence_number);
    BOOST_ASSERT(err == deserialize_errc::ok);
    boost::ignore_unused(err);
    return frame_header{pack.packet_size.value, pack.sequence_number};
}

// Column definition
static column_type compute_field_type_string(std::uint16_t flags, std::uint16_t collation)
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

static column_type compute_field_type_var_string(std::uint16_t collation)
{
    return collation == binary_collation ? column_type::varbinary : column_type::varchar;
}

static column_type compute_field_type_blob(std::uint16_t collation)
{
    return collation == binary_collation ? column_type::blob : column_type::text;
}

static column_type compute_field_type(
    protocol_field_type protocol_type,
    std::uint16_t flags,
    std::uint16_t collation
)
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

error_code boost::mysql::detail::deserialize_column_definition(
    asio::const_buffer input,
    coldef_view& output
) noexcept
{
    column_definition_packet pack{};
    auto err = deserialize_message(input.data(), input.size(), capabilities(), pack);
    if (err)
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
    return error_code();
}

// Command helpers
static void serialize_command_id(boost::asio::mutable_buffer b, std::uint8_t command_id) noexcept
{
    *static_cast<std::uint8_t*>(b.data()) = command_id;
}

static boost::span<std::uint8_t> to_span(boost::asio::mutable_buffer buff) noexcept
{
    return boost::span<std::uint8_t>(static_cast<std::uint8_t*>(buff.data()), buff.size());
}

static boost::span<const std::uint8_t> to_span(string_view data) noexcept
{
    return boost::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(data.data()), data.size());
}

static string_view to_string(boost::span<const std::uint8_t> data) noexcept
{
    return string_view(reinterpret_cast<const char*>(data.data()), data.size());
}

// quit
void boost::mysql::detail::quit_command::serialize(asio::mutable_buffer buff) const noexcept
{
    constexpr std::uint8_t command_id = 0x01;
    serialize_command_id(buff, command_id);
}

// ping
void boost::mysql::detail::ping_command::serialize(asio::mutable_buffer buff) const noexcept
{
    constexpr std::uint8_t command_id = 0x0e;
    serialize_command_id(buff, command_id);
}

error_code boost::mysql::detail::deserialize_ping_response(
    asio::const_buffer message,
    db_flavor flavor,
    diagnostics& diag
)
{
    // Header
    std::uint8_t packet_header{};
    deserialization_context ctx(
        message.data(),
        message.size(),
        capabilities()
    );  // Unaffected by capabilities
    auto err = deserialize_message_part(ctx, packet_header);
    if (err)
        return err;

    if (packet_header == ok_packet_header)
    {
        // Verify that the ok_packet is correct
        ok_packet pack{};
        return deserialize_message(ctx, pack);
    }
    else if (packet_header == error_packet_header)
    {
        // Theoretically, the server can answer with an error packet, too
        return process_error_packet(ctx, flavor, diag);
    }
    else
    {
        // Invalid message
        return make_error_code(client_errc::protocol_value_error);
    }
}

// query
static com_query_packet to_packet(query_command cmd) noexcept
{
    return com_query_packet{string_eof(cmd.query)};
}

std::size_t boost::mysql::detail::query_command::get_size() const noexcept
{
    return get_message_size(to_packet(*this), capabilities());
}

void boost::mysql::detail::query_command::serialize(asio::mutable_buffer buff) const noexcept
{
    serialize_message(to_packet(*this), capabilities(), to_span(buff));
}

// prepare statement
static com_stmt_prepare_packet to_packet(prepare_stmt_command cmd) noexcept { return {string_eof(cmd.stmt)}; }
std::size_t boost::mysql::detail::prepare_stmt_command::get_size() const noexcept
{
    return get_message_size(to_packet(*this), capabilities());
}
void boost::mysql::detail::prepare_stmt_command::serialize(asio::mutable_buffer buff) const noexcept
{
    serialize_message(to_packet(*this), capabilities(), to_span(buff));
}

error_code boost::mysql::detail::deserialize_prepare_stmt_response(
    asio::const_buffer message,
    db_flavor flavor,
    prepare_stmt_response& output,
    diagnostics& diag
)
{
    deserialization_context ctx(message.data(), message.size(), capabilities());
    std::uint8_t msg_type = 0;
    auto err = deserialize_message_part(ctx, msg_type);
    if (err)
        return err;

    if (msg_type == error_packet_header)
    {
        return process_error_packet(ctx, flavor, diag);
    }
    else if (msg_type != 0)
    {
        return make_error_code(client_errc::protocol_value_error);
    }
    else
    {
        com_stmt_prepare_ok_packet response{};
        err = deserialize_message(ctx, response);
        if (err)
            return err;

        output = prepare_stmt_response(response.statement_id, response.num_columns, response.num_params);
        return error_code();
    }
}

// execute statement

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

// Whether to include the unsigned flag in the statement execute message
// for a given value or not. Only value's type is used
static bool is_unsigned(field_view input) noexcept { return input.is_uint64(); }

static constexpr std::size_t param_meta_packet_size = 2;           // type + unsigned flag
static constexpr std::size_t stmt_execute_packet_head_size = 1     // command ID
                                                             + 4   // statement_id
                                                             + 1   // flags
                                                             + 4;  // iteration_count

std::size_t boost::mysql::detail::execute_stmt_command::get_size() const noexcept
{
    serialization_context ctx(capabilities{});
    std::size_t res = stmt_execute_packet_head_size;
    auto num_params = params.size();
    if (num_params > 0u)
    {
        res += null_bitmap_traits(stmt_execute_null_bitmap_offset, num_params).byte_count();
        res += 1;  // new_params_bind_flag
        res += param_meta_packet_size * num_params;
        for (field_view param : params)
        {
            res += ::get_size(ctx, param);
        }
    }

    return res;
}

void boost::mysql::detail::execute_stmt_command::serialize(asio::mutable_buffer buff) const noexcept
{
    constexpr std::uint8_t command_id = 0x17;

    com_stmt_execute_packet_head pack{
        statement_id,
        0,  // flags
        1,  // iteration count
        1,  // new_params_bind_flag
    };

    serialization_context ctx(capabilities(), buff.data());
    ::serialize(ctx, command_id, pack.statement_id, pack.flags, pack.iteration_count);

    // Number of parameters
    auto num_params = params.size();

    if (num_params > 0)
    {
        // NULL bitmap
        null_bitmap_traits traits(stmt_execute_null_bitmap_offset, num_params);
        std::memset(ctx.first(), 0, traits.byte_count());  // Initialize to zeroes
        for (std::size_t i = 0; i < num_params; ++i)
        {
            if (params[i].is_null())
            {
                traits.set_null(ctx.first(), i);
            }
        }
        ctx.advance(traits.byte_count());

        // new parameters bind flag
        ::serialize(ctx, pack.new_params_bind_flag);

        // value metadata
        for (field_view param : params)
        {
            com_stmt_execute_param_meta_packet meta{
                get_protocol_field_type(param),
                is_unsigned(param) ? std::uint8_t(0x80) : std::uint8_t(0)};
            ::serialize(ctx, meta.type, meta.unsigned_flag);
        }

        // actual values
        for (field_view param : params)
        {
            ::serialize(ctx, param);
        }
    }
}

// close statement
std::size_t boost::mysql::detail::close_stmt_command::get_size() const noexcept
{
    return 5;  // ID + statement ID
}

void boost::mysql::detail::close_stmt_command::serialize(asio::mutable_buffer buff) const noexcept
{
    constexpr std::uint8_t command_id = 0x19;
    serialization_context ctx(capabilities(), buff.data());
    ::serialize(ctx, command_id, statement_id);
}

// execute response
static ok_view to_ok_view(const ok_packet& pack) noexcept
{
    return {
        pack.affected_rows.value,
        pack.last_insert_id.value,
        pack.status_flags,
        pack.warnings,
        pack.info.value,
    };
}

execute_response boost::mysql::detail::deserialize_execute_response(
    asio::const_buffer msg,
    db_flavor flavor,
    diagnostics& diag
) noexcept
{
    // Response may be: ok_packet, err_packet, local infile request (not implemented)
    // If it is none of this, then the message type itself is the beginning of
    // a length-encoded int containing the field count
    deserialization_context ctx(msg.data(), msg.size(), capabilities());
    std::uint8_t msg_type = 0;
    auto err = deserialize_message_part(ctx, msg_type);
    if (err)
        return err;

    if (msg_type == ok_packet_header)
    {
        ok_packet ok_pack;
        err = deserialize_message(ctx, ok_pack);
        if (err)
            return err;
        return to_ok_view(ok_pack);
    }
    else if (msg_type == error_packet_header)
    {
        return process_error_packet(ctx, flavor, diag);
    }
    else
    {
        // Resultset with metadata. First packet is an int_lenenc with
        // the number of field definitions to expect. Message type is part
        // of this packet, so we must rewind the context
        ctx.rewind(1);
        int_lenenc num_fields;
        err = deserialize_message(ctx, num_fields);
        if (err)
            return err;

        // We should have at least one field.
        // The max number of fields is some value around 1024. For simplicity/extensibility,
        // we accept anything less than 0xffff
        if (num_fields.value == 0 || num_fields.value > 0xffffu)
        {
            return make_error_code(client_errc::protocol_value_error);
        }

        return static_cast<std::size_t>(num_fields.value);
    }
}

row_message boost::mysql::detail::deserialize_row_message(
    asio::const_buffer msg,
    db_flavor flavor,
    diagnostics& diag
)
{
    // Message type: row, error or eof?
    std::uint8_t msg_type = 0;
    deserialization_context ctx(msg.data(), msg.size(), capabilities());
    auto deser_errc = deserialize(ctx, msg_type);
    if (deser_errc != deserialize_errc::ok)
    {
        return to_error_code(deser_errc);
    }

    if (msg_type == eof_packet_header)
    {
        // end of resultset => this is a ok_packet, not a row
        ok_packet ok_pack{};
        auto err = deserialize_message(ctx, ok_pack);
        if (err)
            return err;
        return to_ok_view(ok_pack);
    }
    else if (msg_type == error_packet_header)
    {
        // An error occurred during the generation of the rows
        return process_error_packet(ctx, flavor, diag);
    }
    else
    {
        // An actual row
        ctx.rewind(1);  // keep the 'message type' byte, as it is part of the actual message
        return span<const std::uint8_t>(ctx.first(), ctx.size());
    }
}

// Deserialize row
static bool is_next_field_null(const deserialization_context& ctx) noexcept
{
    if (!ctx.enough_size(1))
        return false;
    return *ctx.first() == 0xfb;
}

static error_code deserialize_text_row(
    deserialization_context& ctx,
    metadata_collection_view meta,
    field_view* output
)
{
    for (std::vector<field_view>::size_type i = 0; i < meta.size(); ++i)
    {
        if (is_next_field_null(ctx))
        {
            ctx.advance(1);
            output[i] = field_view(nullptr);
        }
        else
        {
            string_lenenc value_str;
            auto err = deserialize(ctx, value_str);
            if (err != deserialize_errc::ok)
                return to_error_code(err);
            err = deserialize_text_field(value_str.value, meta[i], output[i]);
            if (err != deserialize_errc::ok)
                return to_error_code(err);
        }
    }
    if (!ctx.empty())
        return client_errc::extra_bytes;
    return error_code();
}

static error_code deserialize_binary_row(
    deserialization_context& ctx,
    metadata_collection_view meta,
    field_view* output
)
{
    // Skip packet header (it is not part of the message in the binary
    // protocol but it is in the text protocol, so we include it for homogeneity)
    // The caller will have checked we have this byte already for us
    BOOST_ASSERT(ctx.enough_size(1));
    ctx.advance(1);

    // Number of fields
    std::size_t num_fields = meta.size();

    // Null bitmap
    null_bitmap_traits null_bitmap(binary_row_null_bitmap_offset, num_fields);
    const std::uint8_t* null_bitmap_begin = ctx.first();
    if (!ctx.enough_size(null_bitmap.byte_count()))
        return client_errc::incomplete_message;
    ctx.advance(null_bitmap.byte_count());

    // Actual values
    for (std::vector<field_view>::size_type i = 0; i < num_fields; ++i)
    {
        if (null_bitmap.is_null(null_bitmap_begin, i))
        {
            output[i] = field_view(nullptr);
        }
        else
        {
            auto err = deserialize_binary_field(ctx, meta[i], output[i]);
            if (err != deserialize_errc::ok)
                return to_error_code(err);
        }
    }

    // Check for remaining bytes
    if (!ctx.empty())
        return make_error_code(client_errc::extra_bytes);

    return error_code();
}

error_code boost::mysql::detail::deserialize_row(
    resultset_encoding encoding,
    span<const std::uint8_t> buff,
    metadata_collection_view meta,
    span<field_view> output
)
{
    BOOST_ASSERT(meta.size() == output.size());
    deserialization_context ctx(buff.data(), buff.size(), capabilities());
    return encoding == detail::resultset_encoding::text ? deserialize_text_row(ctx, meta, output.data())
                                                        : deserialize_binary_row(ctx, meta, output.data());
}

// Server hello
static db_flavor parse_db_version(string_view version_string) noexcept
{
    return version_string.find("MariaDB") != string_view::npos ? db_flavor::mariadb : db_flavor::mysql;
}

// TODO: improve this
static server_hello::auth_buffer_type convert_buffer(const handshake_packet::auth_buffer_type& input) noexcept
{
    server_hello::auth_buffer_type res;
    std::memcpy(res.data(), input.value().data(), input.value().size());
    return res;
}

error_code boost::mysql::detail::deserialize_server_hello(
    asio::const_buffer msg,
    server_hello& output,
    diagnostics& diag
)
{
    handshake_packet pack;
    deserialization_context ctx(msg.data(), msg.size(), capabilities());

    // Message type
    std::uint8_t msg_type = 0;
    auto err = deserialize_message_part(ctx, msg_type);
    if (err)
        return err;
    if (msg_type == handshake_protocol_version_9)
    {
        return make_error_code(client_errc::server_unsupported);
    }
    else if (msg_type == error_packet_header)
    {
        // We don't know which DB is yet
        return process_error_packet(ctx, db_flavor::mysql, diag);
    }
    else if (msg_type != handshake_protocol_version_10)
    {
        return make_error_code(client_errc::protocol_value_error);
    }

    // Deserialize the handshake packet
    err = deserialize_message(ctx, pack);
    if (err)
        return err;

    // Transform it to a friendlier version
    output = server_hello{
        parse_db_version(pack.server_version.value),
        convert_buffer(pack.auth_plugin_data),
        capabilities(pack.capability_falgs),
        pack.auth_plugin_name.value,
    };
    return error_code();
}

static std::uint8_t get_collation_first_byte(std::uint32_t collation_id) noexcept
{
    return static_cast<std::uint8_t>(collation_id % 0xff);
}

// Login request
static handshake_response_packet to_packet(const login_request& req) noexcept
{
    return handshake_response_packet{
        req.negotiated_capabilities.get(),
        req.max_packet_size,
        get_collation_first_byte(req.collation_id),
        string_null(req.username),
        string_lenenc(to_string(req.auth_response)),
        string_null(req.database),
        string_null(req.auth_plugin_name),
    };
}

std::size_t boost::mysql::detail::login_request::get_size() const noexcept
{
    auto pack = to_packet(*this);
    return get_message_size(pack, capabilities());
}

void boost::mysql::detail::login_request::serialize(asio::mutable_buffer buff) const noexcept
{
    auto pack = to_packet(*this);
    serialize_message(pack, capabilities(), to_span(buff));
}

static ssl_request_packet to_packet(const ssl_request& req) noexcept
{
    return {
        req.negotiated_capabilities.get(),
        req.max_packet_size,
        get_collation_first_byte(req.collation_id),
        {},
    };
}

std::size_t boost::mysql::detail::ssl_request::get_size() const noexcept
{
    auto pack = to_packet(*this);
    return get_message_size(pack, capabilities());
}

void boost::mysql::detail::ssl_request::serialize(asio::mutable_buffer buff) const noexcept
{
    auto pack = to_packet(*this);
    serialize_message(pack, capabilities(), to_span(buff));
}

handhake_server_response boost::mysql::detail::deserialize_handshake_server_response(
    asio::const_buffer buff,
    db_flavor flavor,
    diagnostics& diag
)
{
    deserialization_context ctx(buff.data(), buff.size(), capabilities());
    std::uint8_t msg_type = 0;
    auto err = deserialize_message_part(ctx, msg_type);
    if (err)
        return err;
    if (msg_type == ok_packet_header)
    {
        ok_packet ok{};
        err = deserialize_message(ctx, ok);
        if (err)
            return err;
        return to_ok_view(ok);
    }
    else if (msg_type == error_packet_header)
    {
        return process_error_packet(ctx, flavor, diag);
    }
    else if (msg_type == auth_switch_request_header)
    {
        // We have received an auth switch request. Deserialize it
        auth_switch_request_packet auth_sw{};
        auto err = deserialize_message(ctx, auth_sw);
        if (err)
            return err;
        return auth_switch(auth_sw.plugin_name.value, to_span(auth_sw.auth_plugin_data.value));
    }
    else if (msg_type == auth_more_data_header)
    {
        // We have received an auth more data request. Deserialize it
        auth_more_data_packet more_data;
        auto err = deserialize_message(ctx, more_data);
        if (err)
            return err;

        // If the special value fast_auth_complete_challenge
        // is received as auth data, it means that the auth is complete
        // but we must wait for another OK message. We consider this
        // a special type of message
        string_view challenge = more_data.auth_plugin_data.value;
        if (challenge == fast_auth_complete_challenge)
        {
            return handhake_server_response::ok_follows_t();
        }

        // Otherwise, just return the normal data
        return handhake_server_response(to_span(challenge));
    }
    else
    {
        // Unknown message type
        return make_error_code(client_errc::protocol_value_error);
    }
}

auth_switch_response_packet to_packet(auth_switch_response msg) noexcept
{
    return auth_switch_response_packet{string_eof(to_string(msg.auth_plugin_data))};
}

std::size_t boost::mysql::detail::auth_switch_response::get_size() const noexcept
{
    return get_message_size(to_packet(*this), capabilities());
}

void boost::mysql::detail::auth_switch_response::serialize(asio::mutable_buffer buff) const noexcept
{
    return serialize_message(to_packet(*this), capabilities(), to_span(buff));
}
