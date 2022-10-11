//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_PREPARE_STATEMENT_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_PREPARE_STATEMENT_HPP

#pragma once

#include <boost/mysql/detail/auxiliar/bytestring.hpp>
#include <boost/mysql/detail/channel/channel.hpp>
#include <boost/mysql/detail/network_algorithms/prepare_statement.hpp>
#include <boost/mysql/detail/protocol/capabilities.hpp>
#include <boost/mysql/error.hpp>
#include <boost/mysql/statement_base.hpp>

#include <boost/asio/buffer.hpp>

#include <cstdint>

namespace boost {
namespace mysql {
namespace detail {

class prepare_statement_processor
{
    capabilities caps_;
    bytestring& write_buffer_;
    statement_base& output_;
    error_info& output_info_;
    unsigned remaining_meta_{};

public:
    template <class Stream>
    prepare_statement_processor(
        channel<Stream>& chan,
        statement_base& output,
        error_info& output_info
    ) noexcept
        : caps_(chan.current_capabilities()),
          write_buffer_(chan.shared_buffer()),
          output_(output),
          output_info_(output_info)
    {
    }

    void process_request(boost::string_view statement)
    {
        com_stmt_prepare_packet packet{string_eof(statement)};
        serialize_message(packet, caps_, write_buffer_);
    }

    void process_response(boost::asio::const_buffer message, void* channel, error_code& err)
    {
        deserialization_context ctx(message, caps_);
        std::uint8_t msg_type = 0;
        err = make_error_code(deserialize(ctx, msg_type));
        if (err)
            return;

        if (msg_type == error_packet_header)
        {
            err = process_error_packet(ctx, output_info_);
        }
        else if (msg_type != 0)
        {
            err = make_error_code(errc::protocol_value_error);
        }
        else
        {
            com_stmt_prepare_ok_packet response;
            err = deserialize_message(ctx, response);
            output_.reset(channel, response);
            remaining_meta_ = response.num_columns + response.num_params;
        }
    }

    bool has_remaining_meta() const noexcept { return remaining_meta_ != 0; }
    void on_meta_received() noexcept { --remaining_meta_; }
};

template <class Stream>
struct prepare_statement_op : boost::asio::coroutine
{
    channel<Stream>& chan_;
    prepare_statement_processor processor_;

    prepare_statement_op(channel<Stream>& chan, const prepare_statement_processor& processor)
        : chan_(chan), processor_(processor)
    {
    }

    template <class Self>
    void operator()(Self& self, error_code err = {}, boost::asio::const_buffer read_message = {})
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
            // Write message (already serialized at this point)
            BOOST_ASIO_CORO_YIELD chan_.async_write(
                chan_.shared_buffer(),
                chan_.shared_sequence_number(),
                std::move(self)
            );

            // Read response
            BOOST_ASIO_CORO_YIELD chan_.async_read_one(
                chan_.shared_sequence_number(),
                std::move(self)
            );

            // Process response
            processor_.process_response(read_message, &chan_, err);
            if (err)
            {
                self.complete(err);
                BOOST_ASIO_CORO_YIELD break;
            }

            // Server sends now one packet per parameter and field.
            // We ignore these for now.
            while (processor_.has_remaining_meta())
            {
                // Read from the stream if necessary
                if (!chan_.has_read_messages())
                {
                    BOOST_ASIO_CORO_YIELD chan_.async_read_some(std::move(self));
                }

                // Read the message
                read_message = chan_.next_read_message(chan_.shared_sequence_number(), err);
                if (err)
                {
                    self.complete(err);
                    BOOST_ASIO_CORO_YIELD break;
                }

                // Note it as processed
                processor_.on_meta_received();
            }

            // Complete
            self.complete(error_code());
        }
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

template <class Stream>
void boost::mysql::detail::prepare_statement(
    channel<Stream>& channel,
    boost::string_view statement,
    statement_base& output,
    error_code& err,
    error_info& info
)
{
    // Prepare message
    prepare_statement_processor processor(channel, output, info);
    processor.process_request(statement);

    // Write message
    channel.write(channel.shared_buffer(), channel.shared_sequence_number(), err);
    if (err)
        return;

    // Read response
    auto read_buffer = channel.read_one(channel.shared_sequence_number(), err);
    if (err)
        return;

    // Process response
    processor.process_response(read_buffer, &channel, err);
    if (err)
        return;

    // Server sends now one packet per parameter and field.
    // We ignore these for now.
    while (processor.has_remaining_meta())
    {
        if (!channel.has_read_messages())
        {
            channel.read_some(err);
            if (err)
                return;
        }
        channel.next_read_message(channel.shared_sequence_number(), err);
        if (err)
            return;
    }
}

template <class Stream, class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::detail::async_prepare_statement(
    channel<Stream>& chan,
    boost::string_view statement,
    statement_base& output,
    error_info& info,
    CompletionToken&& token
)
{
    prepare_statement_processor processor(chan, output, info);
    processor.process_request(statement);
    return boost::asio::async_compose<CompletionToken, void(error_code)>(
        prepare_statement_op(chan, processor),
        token,
        chan
    );
}

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_PREPARE_STATEMENT_HPP_ */
