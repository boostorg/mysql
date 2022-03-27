//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_CHANNEL_IMPL_READ_FRAME_HPP
#define BOOST_MYSQL_DETAIL_CHANNEL_IMPL_READ_FRAME_HPP

#pragma once

#include <boost/mysql/detail/auxiliar/bytestring.hpp>
#include <boost/mysql/detail/channel/read_frame_part.hpp>
#include <boost/mysql/error.hpp>
#include <boost/mysql/detail/channel/read_frame.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/coroutine.hpp>


namespace boost {
namespace mysql {
namespace detail {


template<class Stream>
struct read_frame_with_parser_op : boost::asio::coroutine
{
    Stream& stream_;
    frame_part_parser& part_parser_;
    frame_parser& parser_;

    read_frame_with_parser_op(
        Stream& stream,
        frame_part_parser& part_parser,
        frame_parser& parser
    ) :
        stream_(stream),
        part_parser_(part_parser),
        parser_(parser)
    {
    }

    template<class Self>
    void operator()(
        Self& self,
        error_code code = {},
        read_frame_part_result part_result = {}
    )
    {
        // Error checking
        if (code)
        {
            self.complete(code, boost::asio::const_buffer());
            return;
        }

        boost::asio::const_buffer frame;
        BOOST_ASIO_CORO_REENTER(*this)
        {
            while (true)
            {
                BOOST_ASIO_CORO_YIELD async_read_frame_part(stream_, part_parser_, std::move(self));
                frame = parser_.on_frame_part(part_result);
                if (frame.data())
                {
                    self.complete(error_code(), part_result);
                    BOOST_ASIO_CORO_YIELD break;
                }
            }
        }
    }
};

template<class Stream>
struct read_frame_with_bytestring_op : boost::asio::coroutine
{
    Stream& stream_;
    frame_part_parser& part_parser_;
    bytestring& output_buffer_;

    read_frame_with_bytestring_op(
        Stream& stream,
        frame_part_parser& part_parser,
        bytestring& output_buffer
    ) :
        stream_(stream),
        part_parser_(part_parser),
        output_buffer_(output_buffer)
    {
    }

    template<class Self>
    void operator()(
        Self& self,
        error_code code = {},
        read_frame_part_result part_result = {}
    )
    {
        // Error checking
        if (code)
        {
            self.complete(code);
            return;
        }

        BOOST_ASIO_CORO_REENTER(*this)
        {
            while (true)
            {
                BOOST_ASIO_CORO_YIELD async_read_frame_part(stream_, part_parser_, std::move(self));
                append_buffer(output_buffer_, part_result.buffer);
                if (part_result.is_final)
                {
                    self.complete(error_code());
                    BOOST_ASIO_CORO_YIELD break;
                }
            }
        }
    }
};


} // detail
} // mysql
} // boost

inline boost::asio::const_buffer
boost::mysql::detail::frame_parser::on_frame_part(
    const read_frame_part_result& result
) noexcept
{
    if (result.is_final)
    {
        if (message_buffer_.empty())
        {
            // Optimization: small packet that was read in a single call, no need to assemble parts
            return result.buffer;
        }
        else
        {
            append(result.buffer);
            return boost::asio::buffer(message_buffer_);
        }
    }
    else
    {
        append(result.buffer);
        return boost::asio::const_buffer();
    }
}

template <class Stream>
boost::asio::const_buffer
boost::mysql::detail::read_frame(
    Stream& stream,
    frame_part_parser& part_parser,
    frame_parser& parser,
    error_code& err
)
{
    part_parser.reset();
    parser.reset();

    while (true)
    {
        read_frame_part_result result = read_frame_part(stream, part_parser, err);
        if (err)
        {
            return boost::asio::const_buffer(nullptr, 0);
        }
        auto res = parser.on_frame_part(result);
        if (res.data())
        {
            return res;
        }
    }
}

template <class Stream>
void boost::mysql::detail::read_frame(
    Stream& stream,
    frame_part_parser& part_parser,
    bytestring& output_buffer,
    error_code& err
)
{
    part_parser.reset();
    output_buffer.clear();

    while (true)
    {
        read_frame_part_result result = read_frame_part(stream, part_parser, err);
        if (err)
        {
            return;
        }
        append_buffer(output_buffer, result.buffer);
        if (result.is_final)
        {
            return;
        }
    }
}

template <class Stream, class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code, boost::asio::const_buffer))
boost::mysql::detail::async_read_frame(
    Stream& stream,
    frame_part_parser& part_parser,
    frame_parser& parser,
    CompletionToken&& token
)
{
    part_parser.reset();
    parser.reset();
    return boost::asio::async_compose<CompletionToken, void(error_code, boost::asio::const_buffer)>(
        read_frame_with_parser_op<Stream>(stream, part_parser, parser),
        token,
        stream
    );
}

template <class Stream, class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::detail::async_read_frame(
    Stream& stream,
    frame_part_parser& part_parser,
    bytestring& output_buffer,
    CompletionToken&& token
)
{
    part_parser.reset();
    output_buffer.clear();
    return boost::asio::async_compose<CompletionToken, void(error_code)>(
        read_frame_with_bytestring_op<Stream>(stream, part_parser, output_buffer),
        token,
        stream
    );
}


#endif /* INCLUDE_BOOST_MYSQL_DETAIL_AUXILIAR_STATIC_STRING_HPP_ */




