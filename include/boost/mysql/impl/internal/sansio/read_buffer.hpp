//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_READ_BUFFER_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_READ_BUFFER_HPP

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/assert.hpp>
#include <boost/core/span.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>

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
    std::unique_ptr<std::uint8_t[]> buffer_;
    std::size_t size_;
    std::size_t max_size_;
    std::size_t current_message_offset_{0};
    std::size_t pending_offset_{0};
    std::size_t free_offset_{0};

    void do_grow_buffer(std::size_t new_size)
    {
        // TODO: we can implement a better growth strategy here
        BOOST_ASSERT(new_size > size_);
        std::unique_ptr<std::uint8_t[]> new_buffer{new std::uint8_t[new_size]};
        std::memcpy(new_buffer.get(), buffer_.get(), free_offset_);  // Don't copy the free area
        buffer_ = std::move(new_buffer);
        size_ = new_size;
    }

public:
    read_buffer(std::size_t size, std::size_t max_size = static_cast<std::size_t>(-1))
        : buffer_(size > 0 ? new std::uint8_t[size] : nullptr), size_(size), max_size_(max_size)
    {
        // TODO: check that size <= max_size in the caller
        BOOST_ASSERT(size_ <= max_size_);
    }

    void reset() noexcept
    {
        current_message_offset_ = 0;
        pending_offset_ = 0;
        free_offset_ = 0;
    }

    // Whole buffer accessors
    const std::uint8_t* first() const noexcept { return buffer_.get(); }
    std::size_t size() const noexcept { return size_; }

    // Area accessors
    std::uint8_t* reserved_first() noexcept { return buffer_.get(); }
    const std::uint8_t* reserved_first() const noexcept { return buffer_.get(); }
    std::uint8_t* current_message_first() noexcept { return buffer_.get() + current_message_offset_; }
    const std::uint8_t* current_message_first() const noexcept
    {
        return buffer_.get() + current_message_offset_;
    }
    std::uint8_t* pending_first() noexcept { return buffer_.get() + pending_offset_; }
    const std::uint8_t* pending_first() const noexcept { return buffer_.get() + pending_offset_; }
    std::uint8_t* free_first() noexcept { return buffer_.get() + free_offset_; }
    const std::uint8_t* free_first() const noexcept { return buffer_.get() + free_offset_; }

    std::size_t reserved_size() const noexcept { return current_message_offset_; }
    std::size_t current_message_size() const noexcept { return pending_offset_ - current_message_offset_; }
    std::size_t pending_size() const noexcept { return free_offset_ - pending_offset_; }
    std::size_t free_size() const noexcept { return size_ - free_offset_; }

    span<const std::uint8_t> reserved_area() const noexcept { return {reserved_first(), reserved_size()}; }
    span<const std::uint8_t> current_message() const noexcept
    {
        return {current_message_first(), current_message_size()};
    }
    span<const std::uint8_t> pending_area() const noexcept { return {pending_first(), pending_size()}; }
    span<std::uint8_t> free_area() noexcept { return {free_first(), free_size()}; }

    // Moves n bytes from the free to the processing area (e.g. they've been read)
    void move_to_pending(std::size_t length) noexcept
    {
        BOOST_ASSERT(length <= free_size());
        free_offset_ += length;
    }

    // Moves n bytes from the processing to the current message area
    void move_to_current_message(std::size_t length) noexcept
    {
        BOOST_ASSERT(length <= pending_size());
        pending_offset_ += length;
    }

    // Removes the last length bytes from the current message area,
    // effectively memmove'ing all subsequent bytes backwards.
    // Used to remove intermediate headers. length must be > 0
    void remove_current_message_last(std::size_t length) noexcept
    {
        BOOST_ASSERT(length <= current_message_size());
        BOOST_ASSERT(length > 0);
        std::memmove(pending_first() - length, pending_first(), pending_size());
        pending_offset_ -= length;
        free_offset_ -= length;
    }

    // Moves length bytes from the current message area to the reserved area
    // Used to move entire parsed messages or message headers
    void move_to_reserved(std::size_t length) noexcept
    {
        BOOST_ASSERT(length <= current_message_size());
        current_message_offset_ += length;
    }

    // Removes the reserved area, effectively memmove'ing evth backwards
    void remove_reserved() noexcept
    {
        if (reserved_size() > 0)
        {
            // If reserved_size() > 0, these ptrs should never be NULL
            void* to = buffer_.get();
            const void* from = current_message_first();
            BOOST_ASSERT(to != nullptr);
            BOOST_ASSERT(from != nullptr);
            std::size_t currmsg_size = current_message_size();
            std::size_t pend_size = pending_size();
            std::memmove(to, from, currmsg_size + pend_size);
            current_message_offset_ = 0;
            pending_offset_ = currmsg_size;
            free_offset_ = currmsg_size + pend_size;
        }
    }

    // Makes sure the free size is at least n bytes long; resizes the buffer if required
    error_code grow_to_fit(std::size_t n)
    {
        if (free_size() < n)
        {
            std::size_t new_size = size_ + n - free_size();
            if (new_size > max_size_)
                return client_errc::max_buffer_size_exceeded;
            do_grow_buffer(new_size);
        }
        return error_code();
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
