//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "protocol/protocol.hpp"

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/common_server_errc.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/asio/buffer.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/core/span.hpp>

#include <cstddef>

#include "error/server_error_to_string.hpp"
#include "make_string_view.hpp"
#include "protocol/basic_types.hpp"
#include "protocol/capabilities.hpp"
#include "protocol/constants.hpp"
#include "protocol/deserialize_binary_field.hpp"
#include "protocol/deserialize_text_field.hpp"
#include "protocol/null_bitmap_traits.hpp"
#include "protocol/packets.hpp"
#include "protocol/serialization.hpp"

using boost::mysql::client_errc;
using boost::mysql::diagnostics;
using boost::mysql::error_code;
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

// Helper to (de)serialize top-level messages
template <class Serializable>
static void serialize_message(const Serializable& input, boost::asio::mutable_buffer output)
{
    BOOST_ASSERT(output.size() >= get_size(input));
    serialization_context ctx(output.data());
    serialize(ctx, input);
}

template <class Deserializable>
static error_code deserialize_message(deserialization_context ctx, Deserializable& output)
{
    auto err = deserialize(ctx, output);
    if (err != deserialize_errc::ok)
        return to_error_code(err);
    if (!ctx.empty())
        return make_error_code(client_errc::extra_bytes);
    return error_code();
}

template <class Deserializable>
static error_code deserialize_message(const void* buff, std::size_t size, Deserializable& output)
{
    deserialization_context ctx(buff, size);
    return deserialize_message(ctx, output);
}

template <class Deserializable>
static error_code deserialize_message_part(deserialization_context& ctx, Deserializable& output)
{
    return to_error_code(deserialize(ctx, output));
}

// Frame header
namespace {

struct frame_header_packet
{
    int3 packet_size;
    std::uint8_t sequence_number;
};

}  // namespace

void boost::mysql::detail::serialize_frame_header(
    frame_header msg,
    span<std::uint8_t, frame_header_size> buffer
) noexcept
{
    BOOST_ASSERT(msg.size <= 0xffffff);  // range check
    serialization_context ctx(buffer.data());
    frame_header_packet pack{int3{msg.size}, msg.sequence_number};
    serialize(ctx, pack.packet_size, pack.sequence_number);
}

frame_header boost::mysql::detail::deserialize_frame_header(span<const std::uint8_t, frame_header_size> buffer
) noexcept
{
    frame_header_packet pack{};
    deserialization_context ctx(buffer.data(), buffer.size());
    auto err = deserialize(ctx, pack.packet_size, pack.sequence_number);
    BOOST_ASSERT(err == deserialize_errc::ok);
    boost::ignore_unused(err);
    return frame_header{pack.packet_size.value, pack.sequence_number};
}

// Error packets
error_code boost::mysql::detail::process_error_packet(
    span<const std::uint8_t> msg,
    db_flavor flavor,
    diagnostics& diag
)
{
    err_view error_packet{};
    auto code = deserialize_message(msg.data(), msg.size(), error_packet);
    if (code)
        return code;

    // Error message
    access::get_impl(diag).assign_server(error_packet.error_message);

    // Error code
    if (common_error_to_string(error_packet.error_code))
    {
        // This is an error shared between MySQL and MariaDB, represented as a common_server_errc.
        // get_common_error_message will check that the code has a common_server_errc representation
        // (the common error range has "holes" because of removed error codes)
        return static_cast<common_server_errc>(error_packet.error_code);
    }
    else
    {
        // This is a MySQL or MariaDB specific code. There is no fixed list of error codes,
        // as they both keep adding more codes, so no validation happens.
        const auto& cat = flavor == db_flavor::mysql ? get_mysql_server_category()
                                                     : get_mariadb_server_category();
        return error_code(error_packet.error_code, cat);
    }
}
static error_code process_error_packet(deserialization_context ctx, db_flavor flavor, diagnostics& diag)
{
    return process_error_packet(boost::span<const std::uint8_t>(ctx.first(), ctx.size()), flavor, diag);
}

// Column definition
error_code boost::mysql::detail::deserialize_column_definition(
    asio::const_buffer input,
    coldef_view& output
) noexcept
{
    return deserialize_message(input.data(), input.size(), output);
}

// quit
std::size_t boost::mysql::detail::quit_command::get_size() const noexcept { return ::get_size(*this); }
void boost::mysql::detail::quit_command::serialize(asio::mutable_buffer buff) const noexcept
{
    serialize_message(*this, buff);
}

// ping
std::size_t boost::mysql::detail::ping_command::get_size() const noexcept { return ::get_size(*this); }
void boost::mysql::detail::ping_command::serialize(asio::mutable_buffer buff) const noexcept
{
    serialize_message(*this, buff);
}

error_code boost::mysql::detail::deserialize_ping_response(
    asio::const_buffer message,
    db_flavor flavor,
    diagnostics& diag
)
{
    // Header
    std::uint8_t packet_header{};
    deserialization_context ctx(message.data(), message.size());
    auto err = deserialize_message_part(ctx, packet_header);
    if (err)
        return err;

    if (packet_header == ok_packet_header)
    {
        // Verify that the ok_packet is correct
        ok_view ok{};
        return deserialize_message(ctx, ok);
    }
    else if (packet_header == error_packet_header)
    {
        // Theoretically, the server can answer with an error packet, too
        return ::process_error_packet(ctx, flavor, diag);
    }
    else
    {
        // Invalid message
        return client_errc::protocol_value_error;
    }
}

// query
std::size_t boost::mysql::detail::query_command::get_size() const noexcept { return ::get_size(*this); }
void boost::mysql::detail::query_command::serialize(asio::mutable_buffer buff) const noexcept
{
    serialize_message(*this, buff);
}

// prepare statement
std::size_t boost::mysql::detail::prepare_stmt_command::get_size() const noexcept
{
    return ::get_size(*this);
}
void boost::mysql::detail::prepare_stmt_command::serialize(asio::mutable_buffer buff) const noexcept
{
    serialize_message(*this, buff);
}

error_code boost::mysql::detail::deserialize_prepare_stmt_response(
    asio::const_buffer message,
    db_flavor flavor,
    prepare_stmt_response& output,
    diagnostics& diag
)
{
    deserialization_context ctx(message.data(), message.size());
    std::uint8_t msg_type = 0;
    auto err = deserialize_message_part(ctx, msg_type);
    if (err)
        return err;

    if (msg_type == error_packet_header)
    {
        return ::process_error_packet(ctx, flavor, diag);
    }
    else if (msg_type != 0)
    {
        return client_errc::protocol_value_error;
    }
    else
    {
        return deserialize_message(ctx, output);
    }
}

// execute statement
std::size_t boost::mysql::detail::execute_stmt_command::get_size() const noexcept
{
    return ::get_size(*this);
}
void boost::mysql::detail::execute_stmt_command::serialize(asio::mutable_buffer buff) const noexcept
{
    serialize_message(*this, buff);
}

// close statement
std::size_t boost::mysql::detail::close_stmt_command::get_size() const noexcept { return ::get_size(*this); }

void boost::mysql::detail::close_stmt_command::serialize(asio::mutable_buffer buff) const noexcept
{
    serialize_message(*this, buff);
}

// execute response
execute_response boost::mysql::detail::deserialize_execute_response(
    asio::const_buffer msg,
    db_flavor flavor,
    diagnostics& diag
) noexcept
{
    // Response may be: ok_packet, err_packet, local infile request (not implemented)
    // If it is none of this, then the message type itself is the beginning of
    // a length-encoded int containing the field count
    deserialization_context ctx(msg.data(), msg.size());
    std::uint8_t msg_type = 0;
    auto err = deserialize_message_part(ctx, msg_type);
    if (err)
        return err;

    if (msg_type == ok_packet_header)
    {
        ok_view ok{};
        err = deserialize_message(ctx, ok);
        if (err)
            return err;
        return ok;
    }
    else if (msg_type == error_packet_header)
    {
        return ::process_error_packet(ctx, flavor, diag);
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
    deserialization_context ctx(msg.data(), msg.size());
    auto deser_errc = deserialize(ctx, msg_type);
    if (deser_errc != deserialize_errc::ok)
    {
        return to_error_code(deser_errc);
    }

    if (msg_type == eof_packet_header)
    {
        // end of resultset => this is a ok_packet, not a row
        ok_view ok{};
        auto err = deserialize_message(ctx, ok);
        if (err)
            return err;
        return ok;
    }
    else if (msg_type == error_packet_header)
    {
        // An error occurred during the generation of the rows
        return ::process_error_packet(ctx, flavor, diag);
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
    deserialization_context ctx(buff.data(), buff.size());
    return encoding == detail::resultset_encoding::text ? deserialize_text_row(ctx, meta, output.data())
                                                        : deserialize_binary_row(ctx, meta, output.data());
}

// Server hello
error_code boost::mysql::detail::deserialize_server_hello(
    asio::const_buffer msg,
    server_hello& output,
    diagnostics& diag
)
{
    deserialization_context ctx(msg.data(), msg.size());

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
        return ::process_error_packet(ctx, db_flavor::mysql, diag);
    }
    else if (msg_type != handshake_protocol_version_10)
    {
        return make_error_code(client_errc::protocol_value_error);
    }
    else
    {
        return deserialize_message(ctx, output);
    }
}

// Login request
std::size_t boost::mysql::detail::login_request::get_size() const noexcept { return ::get_size(*this); }

void boost::mysql::detail::login_request::serialize(asio::mutable_buffer buff) const noexcept
{
    serialize_message(*this, buff);
}

// ssl_request
std::size_t boost::mysql::detail::ssl_request::get_size() const noexcept { return ::get_size(*this); }

void boost::mysql::detail::ssl_request::serialize(asio::mutable_buffer buff) const noexcept
{
    serialize_message(*this, buff);
}

handhake_server_response boost::mysql::detail::deserialize_handshake_server_response(
    asio::const_buffer buff,
    db_flavor flavor,
    diagnostics& diag
)
{
    deserialization_context ctx(buff.data(), buff.size());
    std::uint8_t msg_type = 0;
    auto err = deserialize_message_part(ctx, msg_type);
    if (err)
        return err;

    if (msg_type == ok_packet_header)
    {
        ok_view ok{};
        err = deserialize_message(ctx, ok);
        if (err)
            return err;
        return ok;
    }
    else if (msg_type == error_packet_header)
    {
        return ::process_error_packet(ctx, flavor, diag);
    }
    else if (msg_type == auth_switch_request_header)
    {
        // We have received an auth switch request. Deserialize it
        auth_switch auth_sw{};
        auto err = deserialize_message(ctx, auth_sw);
        if (err)
            return err;
        return auth_sw;
    }
    else if (msg_type == auth_more_data_header)
    {
        // We have received an auth more data request. Deserialize it
        string_eof auth_more_data;
        auto err = deserialize_message(ctx, auth_more_data);
        if (err)
            return err;

        // If the special value fast_auth_complete_challenge
        // is received as auth data, it means that the auth is complete
        // but we must wait for another OK message. We consider this
        // a special type of message
        string_view challenge = auth_more_data.value;
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

std::size_t boost::mysql::detail::auth_switch_response::get_size() const noexcept
{
    return ::get_size(*this);
}

void boost::mysql::detail::auth_switch_response::serialize(asio::mutable_buffer buff) const noexcept
{
    return serialize_message(*this, buff);
}
