//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_CHANNEL_MESSAGE_WRITER_HPP
#define BOOST_MYSQL_DETAIL_CHANNEL_MESSAGE_WRITER_HPP

#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/channel/any_stream.hpp>
#include <boost/mysql/detail/channel/message_writer_processor.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>

#include <boost/asio/async_result.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/write.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

namespace boost {
namespace mysql {
namespace detail {

class message_writer
{
    message_writer_processor processor_;

    struct write_op;

public:
    message_writer(std::size_t max_frame_size = MAX_PACKET_SIZE) noexcept : processor_(max_frame_size) {}

    asio::mutable_buffer prepare_buffer(std::size_t size, std::uint8_t& seqnum)
    {
        return processor_.prepare_buffer(size, seqnum);
    }

    // Writes an entire message to stream; partitions the message into
    // chunks and adds the required headers
    inline void write(any_stream& stream, error_code& ec);

    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_write(any_stream& stream, CompletionToken&& token);
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#include <boost/mysql/detail/channel/impl/message_writer.hpp>

#endif
