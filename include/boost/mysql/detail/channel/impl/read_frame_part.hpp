//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_CHANNEL_IMPL_READ_FRAME_PART_HPP
#define BOOST_MYSQL_DETAIL_CHANNEL_IMPL_READ_FRAME_PART_HPP

#pragma once

#include <boost/asio/coroutine.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/compose.hpp>
#include <boost/mysql/detail/channel/read_frame_part.hpp>
#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>

namespace boost {
namespace mysql {
namespace detail {


template<class Stream>
struct read_frame_part_op : boost::asio::coroutine
{
    Stream& stream_;
    frame_part_parser& parser_;
    bool has_done_io_ {false};

    read_frame_part_op(
        Stream& stream,
        frame_part_parser& parser
    ) :
        stream_(stream),
        parser_(parser)
    {
    }

    template<class Self>
    void operator()(
        Self& self,
        error_code code = {},
        std::size_t bytes_transferred = 0
    )
    {
        // Error checking
        if (code)
        {
            self.complete(code, read_frame_part_result{});
            return;
        }

        // Non-error path
        frame_part_parser::result variant_result;
        read_frame_part_result* ptr_result;
        error_code* parser_ec;

        BOOST_ASIO_CORO_REENTER(*this)
        {
            while (true)
            {
                variant_result = parser_.on_read(bytes_transferred);

                // Do we have a result yet?
                ptr_result = ::boost::variant2::get_if<read_frame_part_result>(&variant_result);
                if (ptr_result)
                {
                    if (!has_done_io_) // ensure we dispatch the completion through the right executor
                    {
                        BOOST_ASIO_CORO_YIELD ::boost::asio::post(std::move(self));
                    }
                    self.complete(error_code(), *ptr_result);
                    BOOST_ASIO_CORO_YIELD break;
                }

                // Do we have an error?
                parser_ec = boost::variant2::get_if<error_code>(&variant_result);
                if (parser_ec)
                {
                    if (!has_done_io_) // ensure we dispatch the completion through the right executor
                    {
                        BOOST_ASIO_CORO_YIELD ::boost::asio::post(std::move(self));
                    }
                    self.complete(*parser_ec, read_frame_part_result{});
                    BOOST_ASIO_CORO_YIELD break;
                }

                // We need to read more
                has_done_io_ = true;
                BOOST_ASIO_CORO_YIELD stream_.async_read_some(
                    parser_.read_buffer(),
                    std::move(self)
                );
            }
        }
    }
};

} // detail
} // mysql
} // boost

inline boost::mysql::detail::frame_part_parser::result
boost::mysql::detail::frame_part_parser::on_read_impl(
    std::size_t bytes_read
) noexcept
{
    // Make the buffer aware that bytes_read new bytes are available for processing
    buffer_.move_to_processing(bytes_read);

    if (status_ == status::reading_header)
    {
        // If we don't have enough bytes, request more
        if (buffer_.processing_size() < HEADER_SIZE)
        {
            return should_read_more();
        }

        // Deserialize the header
        packet_header header;
        deserialization_context ctx (
            boost::asio::buffer(buffer_.processing_first(), HEADER_SIZE),
            capabilities(0) // unaffected by capabilities
        );
        errc err = deserialize(ctx, header);
        if (err != errc::ok)
        {
            return make_error_code(err);
        }

        // Process the sequence number
        if (header.sequence_number == sequence_number_)
        {
            ++sequence_number_;
        }
        else
        {
            return make_error_code(errc::sequence_number_mismatch);
        }

        // Process the packet size
        remaining_bytes_ = header.packet_size.value;
        more_frames_follow_ = (remaining_bytes_ == MAX_PACKET_SIZE);

        // We are done with the header
        buffer_.remove_from_processing_front(HEADER_SIZE);
        status_ = status::reading_body;
    }

    if (status_ == status::reading_body)
    {
        // We don't have any bytes in the processing area, request more
        if (buffer_.processing_size() == 0)
        {
            return should_read_more{};
        }

        // Update the state
        std::size_t new_bytes = std::min(buffer_.processing_size(), remaining_bytes_);
        remaining_bytes_ -= new_bytes;
        if (remaining_bytes_ == 0)
        {
            status_ = status::reading_header;
        }

        // Move this frame's received bytes to the reserved area
        buffer_.move_to_reserved(new_bytes);

        // Complete
        return read_frame_part_result{
            buffer_.reserved_area(),
            remaining_bytes_ == 0 && !more_frames_follow_
        };
    }

    assert(false); // Should never reach here
}

template <class Stream>
boost::mysql::detail::read_frame_part_result boost::mysql::detail::read_frame_part(
    Stream& stream,
    frame_part_parser& parser,
    error_code& code
)
{
    std::size_t read_size = 0;
    while (true)
    {
        auto variant_result = parser.on_read(read_size);

        // Do we have a result yet?
        auto ptr_result = boost::variant2::get_if<read_frame_part_result>(&variant_result);
        if (ptr_result)
        {
            return *ptr_result;
        }

        // Do we have an error?
        auto ec_result = boost::variant2::get_if<error_code>(&variant_result);
        if (ec_result)
        {
            code = *ec_result;
            return read_frame_part_result{};
        }

        // We need to read more
        read_size = stream.read_some(parser.read_buffer());
    }
}

template <class Stream, class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, read_frame_part_result))
boost::mysql::detail::async_read_frame_part(
    Stream& stream,
    frame_part_parser& parser,
    CompletionToken&& token
)
{
    return boost::asio::async_compose<CompletionToken, void(error_code, read_frame_part_result)>(
        read_frame_part_op<Stream>(stream, parser),
        token,
        stream
    );
}


#endif