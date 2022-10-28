//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_GENERIC_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_GENERIC_HPP

#pragma once

#include <boost/mysql/detail/auxiliar/bytestring.hpp>
#include <boost/mysql/detail/network_algorithms/execute_generic.hpp>
#include <boost/mysql/detail/protocol/capabilities.hpp>
#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>
#include <boost/mysql/error.hpp>
#include <boost/mysql/resultset_base.hpp>

#include <boost/asio/buffer.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>

namespace boost {
namespace mysql {
namespace detail {

class execute_processor
{
    resultset_encoding encoding_;
    resultset_base& output_;
    error_info& output_info_;
    bytestring& write_buffer_;
    capabilities caps_;
    std::size_t num_fields_{};
    std::size_t remaining_fields_{};

public:
    execute_processor(
        resultset_encoding encoding,
        resultset_base& output,
        error_info& output_info,
        bytestring& write_buffer,
        capabilities caps
    ) noexcept
        : encoding_(encoding),
          output_(output),
          output_info_(output_info),
          write_buffer_(write_buffer),
          caps_(caps){};

    void clear_output_info() noexcept { output_info_.clear(); }

    template <BOOST_MYSQL_EXECUTE_REQUEST Request, class Stream>
    void process_request(const Request& request, channel<Stream>& chan)
    {
        output_.reset(&chan, encoding_);
        serialize_message(request, caps_, write_buffer_);
    }

    template <BOOST_MYSQL_EXECUTE_REQUEST_MAKER RequestMaker, class Stream>
    void process_request_maker(const RequestMaker& reqmaker, channel<Stream>& chan)
    {
        auto st = reqmaker.make_storage();
        process_request(reqmaker.make_request(st), chan);
    }

    void process_response(boost::asio::const_buffer response, error_code& err)
    {
        // Response may be: ok_packet, err_packet, local infile request (not implemented)
        // If it is none of this, then the message type itself is the beginning of
        // a length-encoded int containing the field count
        deserialization_context ctx(response, caps_);
        std::uint8_t msg_type = 0;
        err = make_error_code(deserialize(ctx, msg_type));
        if (err)
            return;
        if (msg_type == ok_packet_header)
        {
            ok_packet ok_pack;
            err = deserialize_message(ctx, ok_pack);
            if (err)
                return;
            output_.complete(ok_pack);
            num_fields_ = 0;
        }
        else if (msg_type == error_packet_header)
        {
            err = process_error_packet(ctx, output_info_);
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
                return;

            // For platforms where size_t is shorter than uint64_t,
            // perform range check
            if (num_fields.value > std::numeric_limits<std::size_t>::max())
            {
                err = make_error_code(errc::protocol_value_error);
                return;
            }

            // Ensure we have fields, as field_count is indicative of
            // a resultset with fields
            num_fields_ = static_cast<std::size_t>(num_fields.value);
            if (num_fields_ == 0)
            {
                err = make_error_code(errc::protocol_value_error);
                return;
            }

            remaining_fields_ = num_fields_;
            output_.prepare_meta(num_fields_);
        }
    }

    error_code process_field_definition(boost::asio::const_buffer message)
    {
        column_definition_packet field_definition;
        deserialization_context ctx(message, caps_);
        auto err = deserialize_message(ctx, field_definition);
        if (err)
            return err;

        // Add it to our array
        output_.add_meta(field_definition);
        --remaining_fields_;
        return error_code();
    }

    std::uint8_t& sequence_number() noexcept { return output_.sequence_number(); }
    std::size_t num_fields() const noexcept { return num_fields_; }
    bool has_remaining_fields() const noexcept { return remaining_fields_ != 0; }
};

template <class Stream, BOOST_MYSQL_EXECUTE_REQUEST_MAKER RequestMaker>
struct execute_generic_op : boost::asio::coroutine
{
    channel<Stream>& chan_;
    RequestMaker reqmaker_;
    execute_processor processor_;

    execute_generic_op(
        channel<Stream>& chan,
        RequestMaker&& reqmaker,
        const execute_processor& processor
    )
        : chan_(chan), reqmaker_(std::move(reqmaker)), processor_(processor)
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
            processor_.clear_output_info();

            // Serialize the request
            processor_.process_request_maker(reqmaker_, chan_);

            // Send it
            BOOST_ASIO_CORO_YIELD chan_
                .async_write(chan_.shared_buffer(), processor_.sequence_number(), std::move(self));

            // Read the response
            BOOST_ASIO_CORO_YIELD chan_.async_read_one(
                processor_.sequence_number(),
                std::move(self)
            );

            // Response may be: ok_packet, err_packet, local infile request
            // (not implemented), or response with fields
            processor_.process_response(read_message, err);
            if (err)
            {
                self.complete(err);
                BOOST_ASIO_CORO_YIELD break;
            }

            // Read all of the field definitions
            while (processor_.has_remaining_fields())
            {
                // Read from the stream if we need it
                if (!chan_.has_read_messages())
                {
                    BOOST_ASIO_CORO_YIELD chan_.async_read_some(std::move(self));
                }

                // Read the field definition packet
                read_message = chan_.next_read_message(processor_.sequence_number(), err);
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

template <class Stream, BOOST_MYSQL_EXECUTE_REQUEST Request>
void boost::mysql::detail::execute_generic(
    resultset_encoding encoding,
    channel<Stream>& channel,
    const Request& request,
    resultset_base& output,
    error_code& err,
    error_info& info
)
{
    // Serialize the request
    execute_processor
        processor(encoding, output, info, channel.shared_buffer(), channel.current_capabilities());
    processor.process_request(request, channel);

    // Send it
    channel.write(channel.shared_buffer(), processor.sequence_number(), err);
    if (err)
        return;

    // Read the response
    auto read_buffer = channel.read_one(processor.sequence_number(), err);
    if (err)
        return;

    // Response may be: ok_packet, err_packet, local infile request (not implemented), or response
    // with fields
    processor.process_response(read_buffer, err);
    if (err)
        return;

    // Read all of the field definitions (zero if empty resultset)
    while (processor.has_remaining_fields())
    {
        // Read from the stream if required
        if (!channel.has_read_messages())
        {
            channel.read_some(err);
            if (err)
                return;
        }

        // Read the field definition packet
        read_buffer = channel.next_read_message(processor.sequence_number(), err);
        if (err)
            return;

        // Process the message
        err = processor.process_field_definition(read_buffer);
        if (err)
            return;
    }
}

template <class Stream, BOOST_MYSQL_EXECUTE_REQUEST_MAKER RequestMaker, class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::detail::async_execute_generic(
    resultset_encoding encoding,
    channel<Stream>& channel,
    RequestMaker&& reqmaker,
    resultset_base& output,
    error_info& info,
    CompletionToken&& token
)
{
    return boost::asio::async_compose<CompletionToken, void(error_code)>(
        execute_generic_op<Stream, typename std::decay<RequestMaker>::type>(
            channel,
            std::move(reqmaker),
            execute_processor(
                encoding,
                output,
                info,
                channel.shared_buffer(),
                channel.current_capabilities()
            )
        ),
        token,
        channel
    );
}

#endif /* INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_READ_RESULTSET_HEAD_IPP_ */
