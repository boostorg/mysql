//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_GENERIC_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_GENERIC_HPP

#pragma once

#include <boost/mysql/detail/network_algorithms/execute_generic.hpp>
#include <limits>

namespace boost {
namespace mysql {
namespace detail {

class execute_processor
{
    deserialize_row_fn deserializer_;
    capabilities caps_;
    bytestring buffer_;
    std::size_t field_count_ {};
    ok_packet ok_packet_ {};
    std::vector<field_metadata> fields_;
    std::vector<bytestring> field_buffers_;
public:
    execute_processor(deserialize_row_fn deserializer, capabilities caps):
        deserializer_(deserializer), caps_(caps) {};

    template <class Serializable>
    void process_request(
        const Serializable& request
    )
    {
        serialize_message(request, caps_, buffer_);
    }

    void process_response(
        error_code& err,
        error_info& info
    )
    {
        // Response may be: ok_packet, err_packet, local infile request (not implemented)
        // If it is none of this, then the message type itself is the beginning of
        // a length-encoded int containing the field count
        deserialization_context ctx (boost::asio::buffer(buffer_), caps_);
        std::uint8_t msg_type = 0;
        err = make_error_code(deserialize(ctx, msg_type));
        if (err)
            return;
        if (msg_type == ok_packet_header)
        {
            err = deserialize_message(ctx, ok_packet_);
            if (err)
                return;
            field_count_ = 0;
        }
        else if (msg_type == error_packet_header)
        {
            err = process_error_packet(ctx, info);
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
            field_count_ = static_cast<std::size_t>(num_fields.value);
            if (field_count_ == 0)
            {
                err = make_error_code(errc::protocol_value_error);
                return;
            }

            fields_.reserve(field_count_);
            field_buffers_.reserve(field_count_);
        }
    }

    error_code process_field_definition()
    {
        column_definition_packet field_definition;
        deserialization_context ctx (boost::asio::buffer(buffer_), caps_);
        auto err = deserialize_message(ctx, field_definition);
        if (err)
            return err;

        // Add it to our array
        fields_.push_back(field_definition);
        field_buffers_.push_back(std::move(buffer_));
        buffer_ = bytestring();

        return error_code();
    }

    template <class Stream>
    resultset<Stream> create_resultset(channel<Stream>& chan) &&
    {
        if (field_count_ == 0)
        {
            return resultset<Stream>(
                chan,
                std::move(buffer_),
                ok_packet_
            );
        }
        else
        {
            return resultset<Stream>(
                chan,
                resultset_metadata(std::move(field_buffers_), std::move(fields_)),
                deserializer_
            );
        }
    }

    bytestring& get_buffer() { return buffer_; }

    std::size_t field_count() const noexcept { return field_count_; }
};

template<class Stream>
struct execute_generic_op : boost::asio::coroutine
{
    channel<Stream>& chan_;
    error_info& output_info_;
    std::shared_ptr<execute_processor> processor_;
    std::uint64_t remaining_fields_ {0};

    execute_generic_op(
        channel<Stream>& chan,
        error_info& output_info,
        std::shared_ptr<execute_processor>&& processor
    ) :
        chan_(chan),
        output_info_(output_info),
        processor_(std::move(processor))
    {
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
            self.complete(err, resultset<Stream>());
            return;
        }

        // Non-error path
        BOOST_ASIO_CORO_REENTER(*this)
        {
            chan_.reset_sequence_number();

            // The request message has already been composed in the ctor. Send it
            BOOST_ASIO_CORO_YIELD chan_.async_write(processor_->get_buffer(), std::move(self));

            // Read the response
            BOOST_ASIO_CORO_YIELD chan_.async_read(processor_->get_buffer(), std::move(self));

            // Response may be: ok_packet, err_packet, local infile request
            // (not implemented), or response with fields
            processor_->process_response(err, output_info_);
            if (err)
            {
                self.complete(err, resultset<Stream>());
                BOOST_ASIO_CORO_YIELD break;
            }
            remaining_fields_ = processor_->field_count();

            // Read all of the field definitions
            while (remaining_fields_ > 0)
            {
                // Read the field definition packet
                BOOST_ASIO_CORO_YIELD chan_.async_read(processor_->get_buffer(), std::move(self));

                // Process the message
                err = processor_->process_field_definition();
                if (err)
                {
                    self.complete(err, resultset<Stream>());
                    BOOST_ASIO_CORO_YIELD break;
                }

                remaining_fields_--;
            }

            // No EOF packet is expected here, as we require deprecate EOF capabilities
            self.complete(
                error_code(),
                resultset<Stream>(std::move(*processor_).create_resultset(chan_))
            );
        }
    }
};

} // detail
} // mysql
} // boost

template <class Stream, class Serializable>
void boost::mysql::detail::execute_generic(
    deserialize_row_fn deserializer,
    channel<Stream>& channel,
    const Serializable& request,
    resultset<Stream>& output,
    error_code& err,
    error_info& info
)
{
    // Compose a com_query message, reset seq num
    execute_processor processor (deserializer, channel.current_capabilities());
    processor.process_request(request);
    channel.reset_sequence_number();

    // Send it
    channel.write(boost::asio::buffer(processor.get_buffer()), err);
    if (err)
        return;

    // Read the response
    channel.read(processor.get_buffer(), err);
    if (err)
        return;

    // Response may be: ok_packet, err_packet, local infile request (not implemented), or response with fields
    processor.process_response(err, info);
    if (err)
        return;

    // Read all of the field definitions (zero if empty resultset)
    for (std::uint64_t i = 0; i < processor.field_count(); ++i)
    {
        // Read the field definition packet
        channel.read(processor.get_buffer(), err);
        if (err)
            return;

        // Process the message
        err = processor.process_field_definition();
        if (err)
            return;
    }

    // No EOF packet is expected here, as we require deprecate EOF capabilities
    output = std::move(processor).create_resultset(channel);
}

template <class Stream, class Serializable, class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code, boost::mysql::resultset<Stream>)
)
boost::mysql::detail::async_execute_generic(
    deserialize_row_fn deserializer,
    channel<Stream>& chan,
    const Serializable& request,
    CompletionToken&& token,
    error_info& info
)
{
    auto processor = std::make_shared<execute_processor>(deserializer, chan.current_capabilities());
    processor->process_request(request);
    return boost::asio::async_compose<
        CompletionToken,
        void(error_code, resultset<Stream>)
    >(
        execute_generic_op<Stream>(chan, info, std::move(processor)),
        token,
        chan
    );
}


#endif /* INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_READ_RESULTSET_HEAD_IPP_ */
