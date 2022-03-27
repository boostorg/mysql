//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_CHANNEL_READ_FRAME_HPP
#define BOOST_MYSQL_DETAIL_CHANNEL_READ_FRAME_HPP


#include <boost/mysql/detail/auxiliar/bytestring.hpp>
#include <boost/mysql/detail/channel/read_frame_part.hpp>
#include <boost/asio/buffer.hpp>
#include <cstddef>
#include <cstring>


namespace boost {
namespace mysql {
namespace detail {

inline void append_buffer(
    bytestring& output_buffer,
    boost::asio::const_buffer part
)
{
    std::size_t old_size = output_buffer.size();
    output_buffer.resize(old_size + part.size());
    std::memcpy(&output_buffer[0] + old_size, part.data(), part.size());
}

class frame_parser
{
    bytestring message_buffer_; // used when the message is split in parts and we need to re-assemble it

    void append(boost::asio::const_buffer buff) { append_buffer(message_buffer_, buff); }
public:

    // Returns a null buffer to signal that we need to read more
    inline boost::asio::const_buffer on_frame_part(const read_frame_part_result& result) noexcept;

    void reset() noexcept { message_buffer_.clear(); }
};

template <class Stream>
boost::asio::const_buffer read_frame(
    Stream& stream,
    frame_part_parser& part_parser,
    frame_parser& parser,
    error_code& err
);

template <class Stream, class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, boost::asio::const_buffer))
async_read_frame(
    Stream& stream,
    frame_part_parser& part_parser,
    frame_parser& parser,
    CompletionToken&& token
);


template <class Stream>
void read_frame(
    Stream& stream,
    frame_part_parser& part_parser,
    bytestring& output_buffer,
    error_code& err
);

template <class Stream, class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
async_read_frame(
    Stream& stream,
    frame_part_parser& part_parser,
    bytestring& output_buffer,
    CompletionToken&& token
);


} // detail
} // mysql
} // boost

#include <boost/mysql/detail/channel/impl/read_frame.hpp>

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_AUXILIAR_STATIC_STRING_HPP_ */




