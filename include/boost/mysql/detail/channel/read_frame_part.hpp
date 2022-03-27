//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_CHANNEL_READ_FRAME_PART_HPP
#define BOOST_MYSQL_DETAIL_CHANNEL_READ_FRAME_PART_HPP

#include <boost/asio/async_result.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/mysql/error.hpp>
#include <boost/mysql/detail/channel/read_buffer.hpp>
#include <boost/variant2/variant.hpp>
#include <cstddef>
#include <utility>

namespace boost {
namespace mysql {
namespace detail {


struct read_frame_part_result
{
    boost::asio::const_buffer buffer;
    bool is_final;
};

class frame_parser
{
public:
    struct should_read_more {}; // placeholder telling the caller we need more bytes

    using result = boost::variant2::variant<should_read_more, read_frame_part_result, error_code>;

private:
    enum class status
    {
        reading_header,
        reading_body,
    };

    read_buffer& buffer_;
    std::uint8_t& sequence_number_;
    status status_ {status::reading_header};
    std::size_t remaining_bytes_ {0};
    bool more_frames_follow_ {false};


    inline result on_read_impl(std::size_t bytes_read) noexcept;
public:
    frame_parser(read_buffer& buffer, std::uint8_t& sequence_number) noexcept :
        buffer_(buffer),
        sequence_number_(sequence_number)
    {
    }

    boost::asio::mutable_buffer read_buffer() noexcept { return buffer_.free_area(); }

    void reset()
    {
        assert(status_ == status::reading_header);
        assert(!more_frames_follow_);
        assert(remaining_bytes_ == 0);
        assert(buffer_.reserved_size() == 0);
    }

    // To be called at the beginning and after every read operation. The parser
    // will tell what it needs
    result on_read(std::size_t bytes_read) noexcept
    {
        auto res = on_read_impl(bytes_read);
        if (boost::variant2::holds_alternative<should_read_more>(res))
        {
            buffer_.relocate();
        }
        return res;
    }

    // Marks n bytes as consumed, removing them from the buffer's reserved area.
    // To be called after on_read returns should_complete
    void consume(std::size_t n) noexcept
    {
        buffer_.remove_from_reserved(n);
    }
};

template <class Stream>
void read_frame_part(
    Stream& stream,
    frame_parser& parser,
    read_frame_part_result& result,
    error_code& code
);

template <class Stream, class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
async_read_frame_part(
    Stream& stream,
    frame_parser& parser,
    read_frame_part_result& result,
    CompletionToken&& token
);



} // detail
} // mysql
} // boost

#include <boost/mysql/detail/channel/impl/read_frame_part.hpp>

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_AUXILIAR_STATIC_STRING_HPP_ */




