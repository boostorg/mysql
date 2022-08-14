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
#include <cstdint>
#include <cstring>


namespace boost {
namespace mysql {
namespace detail {


// Custom buffer type optimized for read operations performed in the MySQL protocol.
// The buffer is a single, resizable chunk of memory with four areas:
//   - Reserved area: messages that have already been read but are kept alive,
//     either because we need them or because we haven't cleaned them yet.
//   - Current message area: delimits the message we are currently parsing.
//   - Pending bytes area: bytes we've read but haven't been parsed into a message yet.
//   - Free area: free space for more bytes to be read.
class read_buffer
{
    bytestring buffer_;
    std::size_t current_message_offset_ {0};
    std::size_t pending_offset_ {0};
    std::size_t free_offset_ {0};
public:
    read_buffer(std::size_t size):
        buffer_(size, std::uint8_t(0))
    {
        buffer_.resize(buffer_.capacity());
    }

    // Area accessors
    std::uint8_t* reserved_first() noexcept { return buffer_.data(); }
    const std::uint8_t* reserved_first() const noexcept { return buffer_.data(); }
    std::uint8_t* current_message_first() noexcept { return buffer_.data() + current_message_offset_; }
    const std::uint8_t* current_message_first() const noexcept { return buffer_.data() + current_message_offset_; }
    std::uint8_t* pending_first() noexcept { return buffer_.data() + pending_offset_; }
    const std::uint8_t* pending_first() const noexcept { return buffer_.data() + pending_offset_; }
    std::uint8_t* free_first() noexcept { return buffer_.data() + free_offset_; }
    const std::uint8_t* free_first() const noexcept { return buffer_.data() + free_offset_; }

    std::size_t reserved_size() const noexcept { return pending_offset_; }
    std::size_t current_message_size() const noexcept { return pending_offset_ - current_message_offset_; }
    std::size_t pending_size() const noexcept { return free_offset_ - pending_offset_; }
    std::size_t free_size() const noexcept { return buffer_.size() - free_offset_; }

    boost::asio::const_buffer current_message() const noexcept { return boost::asio::buffer(current_message_first(), current_message_size()); }
    boost::asio::mutable_buffer free_area() noexcept { return boost::asio::buffer(free_first(), free_size()); }

    // Moves length bytes from the current message area to the reserved area
    // Used to move entire parsed messages or message headers
    void move_to_reserved(std::size_t length) noexcept
    {
        assert(length <= current_message_size());
        current_message_offset_ += length;
    }

    // Removes the last length bytes from the current message area,
    // effectively memmove'ing all subsequent bytes backwards.
    // Used to remove intermediate headers.
    void remove_current_message_last(std::size_t length) noexcept
    {
        assert(length <= current_message_size());
        std::memmove(
            pending_first() - length,
            pending_first(),
            pending_size()
        );
        pending_offset_ -= length;
        free_offset_ -= length;
    }

    // Moves n bytes from the processing to the current message area
    void move_to_current_message(std::size_t n) noexcept
    {
        assert(n <= pending_size());
        pending_offset_ += n;
    }

    // Moves n bytes from the free to the processing area (e.g. they've been read)
    void move_to_pending(std::size_t n) noexcept
    {
        assert(n <= free_size());
        free_offset_ += n;
    }

    // Removes the reserved area, effectively memmove'ing evth backwards
    void remove_reserved() noexcept
    {
        if (reserved_size() > 0)
        {
            std::size_t currmsg_size = current_message_size();
            std::size_t pend_size = pending_size();
            std::memmove(
                buffer_.data(),
                current_message_first(),
                currmsg_size + pend_size
            );
            current_message_offset_ = 0;
            pending_offset_ = currmsg_size;
            free_offset_ = currmsg_size + pend_size;
        }
    }

    // Makes sure the free size is at least n bytes long; resizes the buffer if required
    void grow_to_fit(std::size_t n)
    {
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




