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
#include <boost/mysql/detail/protocol/deserialize_execute_response.hpp>
#include <boost/mysql/detail/protocol/execution_state_impl.hpp>
#include <boost/mysql/detail/protocol/process_error_packet.hpp>

#include <boost/asio/buffer.hpp>

namespace boost {
namespace mysql {
namespace detail {

class read_resultset_head_processor
{
    channel_base& chan_;
    execution_state_iface& st_;
    diagnostics& diag_;

public:
    read_resultset_head_processor(channel_base& chan, execution_state_iface& st, diagnostics& diag) noexcept
        : chan_(chan), st_(st), diag_(diag)
    {
    }

    void clear_diag() noexcept { diag_.clear(); }

    error_code process_response(boost::asio::const_buffer msg)
    {
        error_code err;
        auto
            response = deserialize_execute_response(msg, chan_.current_capabilities(), chan_.flavor(), diag_);
        switch (response.type)
        {
        case execute_response::type_t::error: err = response.data.err; break;
        case execute_response::type_t::ok_packet: err = st_.on_head_ok_packet(response.data.ok_pack); break;
        case execute_response::type_t::num_fields: err = st_.on_num_meta(response.data.num_fields); break;
        }
        return err;
    }

    error_code process_field_definition()
    {
        // Read the field definition packet (it's cached at this point)
        assert(chan_.has_read_messages());
        error_code err;
        auto msg = chan_.next_read_message(st_.sequence_number(), err);
        if (err)
            return err;

        // Deserialize
        column_definition_packet field_definition{};
        deserialization_context ctx(msg, chan_.current_capabilities());
        err = deserialize_message(ctx, field_definition);
        if (err)
            return err;

        // Notify the state object
        return st_.on_meta(field_definition, chan_.meta_mode(), diag_);
    }

    channel_base& get_channel() noexcept { return chan_; }
    execution_state_iface& state() noexcept { return st_; }
};

template <class Stream>
struct read_resultset_head_op : boost::asio::coroutine
{
    read_resultset_head_processor processor_;

    read_resultset_head_op(channel<Stream>& chan, execution_state_iface& st, diagnostics& diag)
        : processor_(chan, st, diag)
    {
    }

    channel<Stream>& get_channel() noexcept
    {
        return static_cast<channel<Stream>&>(processor_.get_channel());
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
            processor_.clear_diag();

            // If we're not reading head, return
            if (!processor_.state().should_read_head())
            {
                BOOST_ASIO_CORO_YIELD boost::asio::post(std::move(self));
                self.complete(error_code());
                BOOST_ASIO_CORO_YIELD break;
            }

            // Read the response
            BOOST_ASIO_CORO_YIELD get_channel().async_read_one(
                processor_.state().sequence_number(),
                std::move(self)
            );

            // Response may be: ok_packet, err_packet, local infile request
            // (not implemented), or response with fields
            err = processor_.process_response(read_message);
            if (err)
            {
                self.complete(err);
                BOOST_ASIO_CORO_YIELD break;
            }

            // Read all of the field definitions
            while (processor_.state().should_read_meta())
            {
                // Read from the stream if we need it
                if (!get_channel().has_read_messages())
                {
                    BOOST_ASIO_CORO_YIELD get_channel().async_read_some(std::move(self));
                }

                // Process the metadata packet
                err = processor_.process_field_definition();
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
    execution_state_iface& st,
    error_code& err,
    diagnostics& diag
)
{
    // Setup
    read_resultset_head_processor processor(chan, st, diag);
    processor.clear_diag();

    // If we're not reading head, return
    if (!st.should_read_head())
        return;

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
    while (st.should_read_meta())
    {
        // Read from the stream if required
        if (!chan.has_read_messages())
        {
            chan.read_some(err);
            if (err)
                return;
        }

        // Process the packet
        err = processor.process_field_definition();
        if (err)
            return;
    }
}

template <class Stream, BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::detail::async_read_resultset_head(
    channel<Stream>& channel,
    execution_state_iface& st,
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
