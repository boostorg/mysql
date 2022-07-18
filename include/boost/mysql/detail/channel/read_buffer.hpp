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
//   - Processing area: we have read these bytes and the client hasn't consumed them yet.
//   - Free area: free space for more bytes to be read.
class read_buffer
{
    bytestring buffer_;
    std::size_t processing_offset_;
    std::size_t free_offset_;
public:
    read_buffer(std::size_t size):
        buffer_(size, std::uint8_t(0)),
        processing_offset_(0),
        free_offset_(0)
    {
        buffer_.resize(buffer_.capacity());
    }

    std::uint8_t* processing_first() noexcept { return &buffer_[processing_offset_]; }
    std::uint8_t* free_first() noexcept { return &buffer_[free_offset_]; }

    std::size_t dead_size() const noexcept { return processing_offset_; }
    std::size_t processing_size() const noexcept { return free_offset_ - processing_offset_; }
    std::size_t free_size() const noexcept { return buffer_.size() - free_offset_; }

    boost::asio::mutable_buffer free_area() noexcept { return boost::asio::buffer(free_first(), free_size()); }

    // Removes length bytes from the processing area, at a certain offset
    void remove_from_processing(std::size_t offset, std::size_t length) noexcept
    {
        assert(offset >= processing_offset_);
        assert(length <= processing_size());
        if (offset == 0) // remove from front, just extend the dead area
        {
            processing_offset_ += length;
        }
        else
        {
            // Move remaining bytes backwards
            std::memmove(
                &buffer_[processing_offset_ + offset],
                &buffer_[processing_offset_ + offset + length],
                processing_size() - length
            );
            free_offset_ -= length;
        }

    }

    // Moves n bytes from the free to the processing area (e.g. they've been read)
    void move_to_processing(std::size_t n) noexcept
    {
        assert(n <= free_size());
        free_offset_ += n;
    }

    // Removes the dead area, to make space
    void relocate() noexcept
    {
        if (dead_size() > 0)
        {
            std::memmove(&buffer_[0], processing_first(), processing_size());
            free_offset_ -= dead_size();
            processing_offset_ = 0;
        }
    }

    // Makes sure the free size is at least n bytes long; resizes the buffer if required
    void grow_to_fit(std::size_t n)
    {
        if (free_size() < n)
        {
            relocate();
        }
        if (free_size() < n)
        {
            buffer_.resize(free_size() - n);
            buffer_.resize(buffer_.capacity());
        }
    }

};


} // detail
} // mysql
} // boost



#endif /* INCLUDE_BOOST_MYSQL_DETAIL_AUXILIAR_STATIC_STRING_HPP_ */




