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
#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/network_algorithms/read_resultset_head.hpp>
#include <boost/mysql/detail/protocol/deserialize_execution_messages.hpp>
#include <boost/mysql/detail/protocol/process_error_packet.hpp>

#include <boost/asio/buffer.hpp>
#include <boost/assert.hpp>

namespace boost {
namespace mysql {
namespace detail {

inline error_code process_execution_response(
    channel_base& chan,
    execution_processor& proc,
    boost::asio::const_buffer msg,
    diagnostics& diag
)
{
    auto response = deserialize_execute_response(msg, chan.current_capabilities(), chan.flavor(), diag);
    error_code err;
    switch (response.type)
    {
    case execute_response::type_t::error: err = response.data.err; break;
    case execute_response::type_t::ok_packet:
        err = proc.on_head_ok_packet(response.data.ok_pack, diag);
        break;
    case execute_response::type_t::num_fields: proc.on_num_meta(response.data.num_fields); break;
    }
    return err;
}

inline error_code process_field_definition(channel_base& chan, execution_processor& proc, diagnostics& diag)
{
    // Read the field definition packet (it's cached at this point)
    BOOST_ASSERT(chan.has_read_messages());
    error_code err;
    auto msg = chan.next_read_message(proc.sequence_number(), err);
    if (err)
        return err;

    // Deserialize
    column_definition_packet field_definition{};
    err = deserialize_message(msg, field_definition, chan.current_capabilities());
    if (err)
        return err;

    // Notify the processor
    return proc.on_meta(field_definition, diag);
}

template <class Stream>
struct read_resultset_head_op : boost::asio::coroutine
{
    channel<Stream>& chan_;
    execution_processor& proc_;
    diagnostics& diag_;

    read_resultset_head_op(channel<Stream>& chan, execution_processor& proc, diagnostics& diag)
        : chan_(chan), proc_(proc), diag_(diag)
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

        // Non-error path
        BOOST_ASIO_CORO_REENTER(*this)
        {
            // Setup
            diag_.clear();

            // If we're not reading head, return
            if (!proc_.is_reading_head())
            {
                BOOST_ASIO_CORO_YIELD boost::asio::post(std::move(self));
                self.complete(error_code());
                BOOST_ASIO_CORO_YIELD break;
            }

            // Read the response
            BOOST_ASIO_CORO_YIELD chan_.async_read_one(proc_.sequence_number(), std::move(self));

            // Response may be: ok_packet, err_packet, local infile request
            // (not implemented), or response with fields
            err = process_execution_response(chan_, proc_, read_message, diag_);
            if (err)
            {
                self.complete(err);
                BOOST_ASIO_CORO_YIELD break;
            }

            // Read all of the field definitions
            while (proc_.is_reading_meta())
            {
                // Read from the stream if we need it
                if (!chan_.has_read_messages())
                {
                    BOOST_ASIO_CORO_YIELD chan_.async_read_some(std::move(self));
                }

                // Process the metadata packet
                err = process_field_definition(chan_, proc_, diag_);
                if (err)
                {
                    self.complete(err);
                    BOOST_ASIO_CORO_YIELD break;
                }
            }

            // No EOF packet is expected here, as we require deprecate EOF capabilities
            self.complete(err);
        }
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

template <class Stream>
void boost::mysql::detail::read_resultset_head(
    channel<Stream>& chan,
    execution_processor& proc,
    error_code& err,
    diagnostics& diag
)
{
    // Setup
    err = error_code();
    diag.clear();

    // If we're not reading head, return
    if (!proc.is_reading_head())
        return;

    // Read the response
    auto msg = chan.read_one(proc.sequence_number(), err);
    if (err)
        return;

    // Response may be: ok_packet, err_packet, local infile request
    // (not implemented), or response with fields
    err = process_execution_response(chan, proc, msg, diag);
    if (err)
        return;

    // Read all of the field definitions (zero if empty resultset)
    while (proc.is_reading_meta())
    {
        // Read from the stream if required
        if (!chan.has_read_messages())
        {
            chan.read_some(err);
            if (err)
                return;
        }

        // Process the packet
        err = process_field_definition(chan, proc, diag);
        if (err)
            return;
    }
}

template <class Stream, BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::detail::async_read_resultset_head(
    channel<Stream>& channel,
    execution_processor& proc,
    diagnostics& diag,
    CompletionToken&& token
)
{
    return boost::asio::async_compose<CompletionToken, void(error_code)>(
        read_resultset_head_op<Stream>(channel, proc, diag),
        token,
        channel
    );
}

#endif
