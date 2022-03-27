//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_CHANNEL_READ_BUFFER_HPP
#define BOOST_MYSQL_DETAIL_CHANNEL_READ_BUFFER_HPP

#include <boost/asio/buffer.hpp>
#include <boost/mysql/detail/auxiliar/bytestring.hpp>
#include <cstddef>
#include <cassert>
#include <cstring>


namespace boost {
namespace mysql {
namespace detail {


// Custom buffer type optimized for read operations performed in the MySQL protocol.
// The buffer is a single, fixed-size chunk of memory with four areas:
//   - Dead area: these bytes have already been consumed, but won't disappear until a call to relocate().
//     Optimized the case where we read several entire messages in a single read_some() call, preventing
//     several memcpy()'s.
//   - Reserved area: we already processed these bytes, but the client component hasn't consumed them yet.
//   - Processing area: we have read these bytes but haven't processed them yet.
//   - Free area: free space for more bytes to be read.
class read_buffer
{
    bytestring buffer_;
    std::size_t reserved_offset_;
    std::size_t processing_offset_;
    std::size_t free_offset_;
public:
    read_buffer(std::size_t size):
        buffer_(size, std::uint8_t(0)),
        reserved_offset_(0),
        processing_offset_(0),
        free_offset_(0)
    {
        buffer_.resize(buffer_.capacity());
    }

    std::uint8_t* reserved_first() noexcept { return &buffer_[reserved_offset_]; }
    std::uint8_t* processing_first() noexcept { return &buffer_[processing_offset_]; }
    std::uint8_t* free_first() noexcept { return &buffer_[free_offset_]; }

    std::size_t reserved_size() const noexcept { return processing_offset_ - reserved_offset_; }
    std::size_t processing_size() const noexcept { return free_offset_ - processing_offset_; }
    std::size_t free_size() const noexcept { return buffer_.size() - free_offset_; }

    boost::asio::mutable_buffer reserved_area() noexcept { return boost::asio::buffer(reserved_first(), reserved_size()); }
    boost::asio::mutable_buffer free_area() noexcept { return boost::asio::buffer(free_first(), free_size()); }

    // Removes n bytes from the reserved area
    void remove_from_reserved(std::size_t n) noexcept
    {
        assert(n <= reserved_size());
        reserved_offset_ += n;
    }

    // Removes n bytes from the read area, without touching the reserved area
    void remove_from_processing_front(std::size_t n) noexcept
    {
        assert(n <= processing_size());
        if (reserved_size())
        {
            // Move the reserved bytes to the front
            std::memmove(reserved_first() + n, reserved_first(), n);
        }
        reserved_offset_ += n;
        processing_offset_ += n;
    }

    // Moves n bytes from the processing area to the reserved area
    void move_to_reserved(std::size_t n) noexcept
    {
        assert(n <= processing_size());
        processing_offset_ += n;
    }

    // Moves n bytes from the free area to the processing area
    void move_to_processing(std::size_t n) noexcept
    {
        assert(n <= free_size());
        free_offset_ += n;
    }

    // Removes the dead area, to make space
    void relocate() noexcept
    {
        if (reserved_offset_ > 0)
        {
            std::memmove(&buffer_[0], reserved_first(), free_offset_ - reserved_offset_);
            processing_offset_ -= reserved_offset_;
            free_offset_ -= reserved_offset_;
            reserved_offset_ = 0;
        }
    }

};


} // detail
} // mysql
} // boost



#endif /* INCLUDE_BOOST_MYSQL_DETAIL_AUXILIAR_STATIC_STRING_HPP_ */




