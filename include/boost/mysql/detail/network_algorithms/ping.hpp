//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_PING_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_PING_HPP

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/channel/channel.hpp>
#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/protocol/capabilities.hpp>
#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>
#include <boost/mysql/detail/protocol/deserialization_context.hpp>
#include <boost/mysql/detail/protocol/process_error_packet.hpp>
#include <boost/mysql/detail/protocol/serialization.hpp>

#include <boost/asio/async_result.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/coroutine.hpp>

namespace boost {
namespace mysql {
namespace detail {

inline void serialize_ping_message(channel& chan)
{
    chan.serialize(ping_packet{}, chan.reset_sequence_number());
}

inline error_code process_ping_response(
    boost::asio::const_buffer buff,
    capabilities caps,
    db_flavor flavor,
    diagnostics& diag
)
{
    // Header
    std::uint8_t packet_header{};
    deserialization_context ctx(buff, caps);
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

struct ping_op : boost::asio::coroutine
{
    channel& chan_;
    diagnostics& diag_;

    ping_op(channel& chan, diagnostics& diag) noexcept : chan_(chan), diag_(diag) {}

    template <class Self>
    void operator()(Self& self, error_code err = {}, boost::asio::const_buffer buff = {})
    {
        // Error checking
        if (err)
        {
            self.complete(err);
            return;
        }

        // Regular coroutine body; if there has been an error, we don't get here
        BOOST_ASIO_CORO_REENTER(*this)
        {
            diag_.clear();

            // Serialize the message
            serialize_ping_message(chan_);

            // Write message
            BOOST_ASIO_CORO_YIELD chan_.async_write(std::move(self));

            // Read response
            BOOST_ASIO_CORO_YIELD chan_.async_read_one(chan_.shared_sequence_number(), std::move(self));

            // Verify it's what we expected
            self.complete(process_ping_response(buff, chan_.current_capabilities(), chan_.flavor(), diag_));
        }
    }
};

// Interface
inline void ping_impl(channel& chan, error_code& code, diagnostics& diag)
{
    // Serialize the message
    serialize_ping_message(chan);

    // Send it
    chan.write(code);
    if (code)
        return;

    // Read response
    auto response = chan.read_one(chan.shared_sequence_number(), code);
    if (code)
        return;

    // Verify it's what we expected
    code = process_ping_response(response, chan.current_capabilities(), chan.flavor(), diag);
}

template <class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
async_ping_impl(channel& chan, diagnostics& diag, CompletionToken&& token)
{
    return asio::async_compose<CompletionToken, void(error_code)>(ping_op(chan, diag), token, chan);
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
