//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_PREPARE_STATEMENT_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_PREPARE_STATEMENT_HPP

#pragma once

#include <boost/mysql/detail/network_algorithms/prepare_statement.hpp>


namespace boost {
namespace mysql {
namespace detail {

template <class Stream>
class prepare_statement_processor
{
    channel<Stream>& channel_;
    com_stmt_prepare_ok_packet response_ {};
public:
    prepare_statement_processor(channel<Stream>& chan): channel_(chan) {}
    void process_request(boost::string_view statement)
    {
        com_stmt_prepare_packet packet { string_eof(statement) };
        serialize_message(packet, channel_.current_capabilities(), channel_.shared_buffer());
        channel_.reset_sequence_number();
    }
    void process_response(error_code& err, error_info& info)
    {
        deserialization_context ctx (
            boost::asio::buffer(channel_.shared_buffer()),
            channel_.current_capabilities()
        );
        std::uint8_t msg_type = 0;
        err = make_error_code(deserialize(ctx, msg_type));
        if (err)
            return;

        if (msg_type == error_packet_header)
        {
            err = process_error_packet(ctx, info);
        }
        else if (msg_type != 0)
        {
            err = make_error_code(errc::protocol_value_error);
        }
        else
        {
            err = deserialize_message(ctx, response_);
        }
    }
    bytestring& get_buffer() noexcept { return channel_.shared_buffer(); }
    channel<Stream>& get_channel() noexcept { return channel_; }
    const com_stmt_prepare_ok_packet& get_response() const noexcept { return response_; }

    unsigned get_num_metadata_packets() const noexcept
    {
        return response_.num_columns + response_.num_params;
    }
};

template<class Stream>
struct prepare_statement_op : boost::asio::coroutine
{
    prepare_statement_processor<Stream> processor_;
    error_info& output_info_;
    unsigned remaining_meta_ {0};

    prepare_statement_op(
        channel<Stream>& chan,
        error_info& output_info,
        boost::string_view statement
    ) :
        processor_(chan),
        output_info_(output_info)
    {
        processor_.process_request(statement);
    }

    template<class Self>
    void operator()(
        Self& self,
        error_code err = {}
    )
    {
        // Error checking
        if (err)
        {
            self.complete(err, prepared_statement<Stream>());
            return;
        }

        // Regular coroutine body; if there has been an error, we don't get here
        channel<Stream>& chan = processor_.get_channel();
        BOOST_ASIO_CORO_REENTER(*this)
        {
            // Write message (already serialized at this point)
            BOOST_ASIO_CORO_YIELD chan.async_write(processor_.get_buffer(), std::move(self));

            // Read response
            BOOST_ASIO_CORO_YIELD chan.async_read(processor_.get_buffer(), std::move(self));

            // Process response
            processor_.process_response(err, output_info_);
            if (err)
            {
                self.complete(err, prepared_statement<Stream>());
                BOOST_ASIO_CORO_YIELD break;
            }

            // Server sends now one packet per parameter and field.
            // We ignore these for now.
            remaining_meta_ = processor_.get_num_metadata_packets();
            for (; remaining_meta_ > 0; --remaining_meta_)
            {
                BOOST_ASIO_CORO_YIELD chan.async_read(processor_.get_buffer(), std::move(self));
            }

            // Compose response
            self.complete(
                err,
                prepared_statement<Stream>(processor_.get_channel(), processor_.get_response())
            );
        }
    }
};

} // detail
} // mysql
} // boost

template <class Stream>
void boost::mysql::detail::prepare_statement(
    channel<Stream>& channel,
    boost::string_view statement,
    error_code& err,
    error_info& info,
    prepared_statement<Stream>& output
)
{
    // Prepare message
    prepare_statement_processor<Stream> processor (channel);
    processor.process_request(statement);

    // Write message
    processor.get_channel().write(boost::asio::buffer(processor.get_buffer()), err);
    if (err)
        return;

    // Read response
    processor.get_channel().read(processor.get_buffer(), err);
    if (err)
        return;

    // Process response
    processor.process_response(err, info);
    if (err)
        return;

    // Server sends now one packet per parameter and field.
    // We ignore these for now.
    for (unsigned i = 0; i < processor.get_num_metadata_packets(); ++i)
    {
        processor.get_channel().read(processor.get_buffer(), err);
        if (err)
            return;
    }

    // Compose response
    output = prepared_statement<Stream>(channel, processor.get_response());
}

template <class Stream, class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code, boost::mysql::prepared_statement<Stream>)
)
boost::mysql::detail::async_prepare_statement(
    channel<Stream>& chan,
    boost::string_view statement,
    CompletionToken&& token,
    error_info& info
)
{
    return boost::asio::async_compose<
        CompletionToken,
        void(error_code, prepared_statement<Stream>)
    >(
        prepare_statement_op<Stream>{chan, info, statement},
        token,
        chan
    );
}

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_PREPARE_STATEMENT_HPP_ */
