//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_READ_ROW_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_READ_ROW_HPP

#include "boost/mysql/detail/protocol/text_deserialization.hpp"

namespace boost {
namespace mysql {
namespace detail {

inline read_row_result process_read_message(
    deserialize_row_fn deserializer,
    capabilities current_capabilities,
    const std::vector<field_metadata>& meta,
    const bytestring& buffer,
    std::vector<value>& output_values,
    ok_packet& output_ok_packet,
    error_code& err,
    error_info& info
)
{
    assert(deserializer);

    // Message type: row, error or eof?
    std::uint8_t msg_type = 0;
    deserialization_context ctx (boost::asio::buffer(buffer), current_capabilities);
    err = make_error_code(deserialize(ctx, msg_type));
    if (err)
        return read_row_result::error;
    if (msg_type == eof_packet_header)
    {
        // end of resultset
        err = deserialize_message(ctx, output_ok_packet);
        return err ? read_row_result::error : read_row_result::eof;
        if (err)
            return read_row_result::error;
        return read_row_result::eof;
    }
    else if (msg_type == error_packet_header)
    {
        // An error occurred during the generation of the rows
        err = process_error_packet(ctx, info);
        return read_row_result::error;
    }
    else
    {
        // An actual row
        ctx.rewind(1); // keep the 'message type' byte, as it is part of the actual message
        err = deserializer(ctx, meta, output_values);
        if (err)
            return read_row_result::error;
        return read_row_result::row;
    }
}

template<class Stream>
struct read_row_op : boost::asio::coroutine
{
    channel<Stream>& chan_;
    error_info& output_info_;
    deserialize_row_fn deserializer_;
    const std::vector<field_metadata>& meta_;
    bytestring& buffer_;
    std::vector<value>& output_values_;
    ok_packet& output_ok_packet_;

    read_row_op(
        channel<Stream>& chan,
        error_info& output_info,
        deserialize_row_fn deserializer,
        const std::vector<field_metadata>& meta,
        bytestring& buffer,
        std::vector<value>& output_values,
        ok_packet& output_ok_packet
    ) :
        chan_(chan),
        output_info_(output_info),
        deserializer_(deserializer),
        meta_(meta),
        buffer_(buffer),
        output_values_(output_values),
        output_ok_packet_(output_ok_packet)
    {
    }

    template<class Self>
    void operator()(
        Self& self,
        error_code err = {}
    )
    {
        read_row_result result = read_row_result::error;

        // Error checking
        if (err)
        {
            self.complete(err, result);
            return;
        }

        // Normal path
        BOOST_ASIO_CORO_REENTER(*this)
        {
            // Read the message
            BOOST_ASIO_CORO_YIELD chan_.async_read(buffer_, std::move(self));

            // Process it
            result = process_read_message(
                deserializer_,
                chan_.current_capabilities(),
                meta_,
                buffer_,
                output_values_,
                output_ok_packet_,
                err,
                output_info_
            );
            self.complete(err, result);
        }
    }
};

} // detail
} // mysql
} // boost


template <class Stream>
boost::mysql::detail::read_row_result boost::mysql::detail::read_row(
    deserialize_row_fn deserializer,
    channel<Stream>& channel,
    const std::vector<field_metadata>& meta,
    bytestring& buffer,
    std::vector<value>& output_values,
    ok_packet& output_ok_packet,
    error_code& err,
    error_info& info
)
{
    // Read a packet
    channel.read(buffer, err);
    if (err)
        return read_row_result::error;

    return process_read_message(
        deserializer,
        channel.current_capabilities(),
        meta,
        buffer,
        output_values,
        output_ok_packet,
        err,
        info
    );
}

template <class Stream, class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code, boost::mysql::detail::read_row_result)
)
boost::mysql::detail::async_read_row(
    deserialize_row_fn deserializer,
    channel<Stream>& chan,
    const std::vector<field_metadata>& meta,
    bytestring& buffer,
    std::vector<value>& output_values,
    ok_packet& output_ok_packet,
    CompletionToken&& token,
    error_info& output_info
)
{
    return boost::asio::async_compose<CompletionToken, void(error_code, read_row_result)> (
        read_row_op<Stream>(
            chan,
            output_info,
            deserializer,
            meta,
            buffer,
            output_values,
            output_ok_packet
        ),
        token,
        chan
    );
}

#endif /* INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_READ_TEXT_ROW_IPP_ */
