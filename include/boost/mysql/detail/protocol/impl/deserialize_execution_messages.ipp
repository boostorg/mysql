//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_DESERIALIZE_EXECUTION_MESSAGES_IPP
#define BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_DESERIALIZE_EXECUTION_MESSAGES_IPP

#pragma once

#include <boost/mysql/detail/protocol/deserialize_errc.hpp>
#include <boost/mysql/detail/protocol/deserialize_execution_messages.hpp>
#include <boost/mysql/detail/protocol/process_error_packet.hpp>

namespace boost {
namespace mysql {
namespace detail {

}  // namespace detail
}  // namespace mysql
}  // namespace boost

inline boost::mysql::detail::execute_response boost::mysql::detail::deserialize_execute_response(
    boost::asio::const_buffer msg,
    capabilities caps,
    db_flavor flavor,
    diagnostics& diag
) noexcept
{
    // Response may be: ok_packet, err_packet, local infile request (not implemented)
    // If it is none of this, then the message type itself is the beginning of
    // a length-encoded int containing the field count
    deserialization_context ctx(msg, caps);
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
        return ok_pack;
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

inline boost::mysql::detail::row_message boost::mysql::detail::deserialize_row_message(
    boost::asio::const_buffer msg,
    capabilities caps,
    db_flavor flavor,
    diagnostics& diag
)
{
    // Message type: row, error or eof?
    std::uint8_t msg_type = 0;
    deserialization_context ctx(msg, caps);
    auto deser_errc = deserialize(ctx, msg_type);
    if (deser_errc != deserialize_errc::ok)
    {
        return to_error_code(deser_errc);
    }

    if (msg_type == eof_packet_header)
    {
        // end of resultset => this is a ok_packet, not a row
        ok_packet ok_pack;
        auto err = deserialize_message(ctx, ok_pack);
        if (err)
            return err;
        return ok_pack;
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
        return ctx;
    }
}

inline boost::mysql::detail::row_message boost::mysql::detail::deserialize_row_message(
    channel_base& chan,
    std::uint8_t& sequence_number,
    diagnostics& diag
)
{
    // Get the row message
    error_code err;
    auto buff = chan.next_read_message(sequence_number, err);
    if (err)
        return err;

    // Deserialize it
    return deserialize_row_message(buff, chan.current_capabilities(), chan.flavor(), diag);
}

#endif
