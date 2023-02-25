//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_READ_RESULTSET_HEAD_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_READ_RESULTSET_HEAD_HPP

#pragma once

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/channel/channel.hpp>
#include <boost/mysql/detail/network_algorithms/read_resultset_head.hpp>
#include <boost/mysql/detail/protocol/execution_state_impl.hpp>
#include <boost/mysql/detail/protocol/process_error_packet.hpp>

#include <boost/asio/buffer.hpp>

namespace boost {
namespace mysql {
namespace detail {

class read_resultset_head_processor
{
    channel_base& chan_;
    execution_state_impl& st_;
    diagnostics& diag_;

public:
    read_resultset_head_processor(channel_base& chan, execution_state_impl& st, diagnostics& diag) noexcept
        : chan_(chan), st_(st), diag_(diag)
    {
    }

    void setup() noexcept
    {
        assert(st_.should_read_head());
        diag_.clear();
    }

    error_code process_response(boost::asio::const_buffer msg)
    {
        auto
            response = deserialize_execute_response(msg, chan_.current_capabilities(), chan_.flavor(), diag_);
        switch (response.type)
        {
        case execute_response::type_t::error: return response.data.err;
        case execute_response::type_t::ok_packet:
            st_.on_ok_packet(response.data.ok_pack);
            return error_code();
        case execute_response::type_t::num_fields:
            st_.on_num_meta(response.data.num_fields);
            return error_code();
        }
    }

    error_code process_field_definition(boost::asio::const_buffer message)
    {
        column_definition_packet field_definition{};
        deserialization_context ctx(message, chan_.current_capabilities());
        auto err = deserialize_message(ctx, field_definition);
        if (err)
            return err;

        st_.on_meta(field_definition, chan_.meta_mode());
        return error_code();
    }

    channel_base& get_channel() noexcept { return chan_; }
    std::uint8_t& sequence_number() noexcept { return st_.sequence_number(); }
    bool has_remaining_meta() const noexcept { return st_.has_remaining_meta(); }
};

template <class Stream>
struct read_resultset_head_op : boost::asio::coroutine
{
    read_resultset_head_processor processor_;

    read_resultset_head_op(channel<Stream>& chan, execution_state_impl& st, diagnostics& diag)
        : processor_(chan, st, diag)
    {
    }

    channel<Stream> get_channel() noexcept { return static_cast<channel<Stream>>(processor_.get_channel()); }

    template <class Self>
    void operator()(Self& self, error_code err = {}, boost::asio::const_buffer read_message = {})
    {
        // Error checking
        if (err)
        {
            self.complete(err);
            return;
        }

        // Non-error path
        BOOST_ASIO_CORO_REENTER(*this)
        {
            // Setup
            processor_.setup();

            // Read the response
            BOOST_ASIO_CORO_YIELD get_channel().async_read_one(processor_.sequence_number(), std::move(self));

            // Response may be: ok_packet, err_packet, local infile request
            // (not implemented), or response with fields
            err = processor_.process_response(read_message);
            if (err)
            {
                self.complete(err);
                BOOST_ASIO_CORO_YIELD break;
            }

            // Read all of the field definitions
            while (processor_.has_remaining_meta())
            {
                // Read from the stream if we need it
                if (!get_channel().has_read_messages())
                {
                    BOOST_ASIO_CORO_YIELD get_channel().async_read_some(std::move(self));
                }

                // Read the field definition packet
                read_message = get_channel().next_read_message(processor_.sequence_number(), err);
                if (err)
                {
                    self.complete(err);
                    BOOST_ASIO_CORO_YIELD break;
                }

                // Process the message
                err = processor_.process_field_definition(read_message);
                if (err)
                {
                    self.complete(err);
                    BOOST_ASIO_CORO_YIELD break;
                }
            }

            // No EOF packet is expected here, as we require deprecate EOF capabilities
            self.complete(error_code());
        }
    }
};

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

template <class Stream>
void boost::mysql::detail::read_resultset_head(
    channel<Stream>& chan,
    execution_state_impl& st,
    error_code& err,
    diagnostics& diag
)
{
    // Setup
    read_resultset_head_processor processor(chan, st, diag);
    processor.setup();

    // Read the response
    auto msg = chan.read_one(st.sequence_number(), err);
    if (err)
        return;

    // Response may be: ok_packet, err_packet, local infile request
    // (not implemented), or response with fields
    err = processor.process_response(msg);
    if (err)
        return;

    // Read all of the field definitions (zero if empty resultset)
    while (st.has_remaining_meta())
    {
        // Read from the stream if required
        if (!chan.has_read_messages())
        {
            chan.read_some(err);
            if (err)
                return;
        }

        // Read the field definition packet
        msg = chan.next_read_message(st.sequence_number(), err);
        if (err)
            return;

        // Process the message
        err = processor.process_field_definition(msg);
        if (err)
            return;
    }
}

template <class Stream, BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::detail::async_read_resultset_head(
    channel<Stream>& channel,
    execution_state_impl& st,
    diagnostics& diag,
    CompletionToken&& token
)
{
    return boost::asio::async_compose<CompletionToken, void(error_code)>(
        read_resultset_head_op<Stream>(channel, st, diag),
        token,
        channel
    );
}

#endif
