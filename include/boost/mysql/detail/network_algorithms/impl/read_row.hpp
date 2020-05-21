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
    std::uint8_t msg_type;
    deserialization_context ctx (boost::asio::buffer(buffer), current_capabilities);
    std::tie(err, msg_type) = deserialize_message_type(ctx);
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

} // detail
} // mysql
} // boost


template <typename StreamType>
boost::mysql::detail::read_row_result boost::mysql::detail::read_row(
    deserialize_row_fn deserializer,
    channel<StreamType>& channel,
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

namespace boost{ namespace mysql { namespace detail {

template<class StreamType>
struct read_row_op : async_op<StreamType>
{
  deserialize_row_fn deserializer_;
  const std::vector<field_metadata>& meta_;
  bytestring& buffer_;
  std::vector<value>& output_values_;
  ok_packet& output_ok_packet_;

  read_row_op(
  channel<StreamType>& chan,
      error_info* output_info,
  deserialize_row_fn deserializer,
  const std::vector<field_metadata>& meta,
      bytestring& buffer,
  std::vector<value>& output_values,
      ok_packet& output_ok_packet
  ) :
  async_op<StreamType>(chan, output_info),
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
    error_info info;
    read_row_result result = read_row_result::error;

    // Error checking
    if (err)
    {
      this->complete(self, err, result);
      return;
    }

    // Normal path
    BOOST_ASIO_CORO_REENTER(*this)
      {
        // Read the message
        BOOST_ASIO_CORO_YIELD this->async_read(std::move(self), buffer_);

        // Process it
        result = process_read_message(
            deserializer_,
            this->get_channel().current_capabilities(),
            meta_,
            buffer_,
            output_values_,
            output_ok_packet_,
            err,
            info
        );
        detail::conditional_assign(this->get_output_info(), std::move(info));
        this->complete(self, err, result);
      }
  }
};

}}}

template <typename StreamType, typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(
    CompletionToken,
    boost::mysql::detail::read_row_signature
)
boost::mysql::detail::async_read_row(
    deserialize_row_fn deserializer,
    channel<StreamType>& chan,
    const std::vector<field_metadata>& meta,
    bytestring& buffer,
    std::vector<value>& output_values,
    ok_packet& output_ok_packet,
    CompletionToken&& token,
    error_info* output_info
)
{
    return boost::asio::async_compose<CompletionToken, read_row_signature>(
        read_row_op(
            chan,
            output_info,
            deserializer,
            meta,
            buffer,
            output_values,
            output_ok_packet
            ), token, chan
        );

//    return op::initiate(
//        std::forward<CompletionToken>(token),
//        chan,
//        output_info,
//        deserializer,
//        meta,
//        buffer,
//        output_values,
//        output_ok_packet
//    );
}

#endif /* INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_READ_TEXT_ROW_IPP_ */
