//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_IMPL_SERIALIZATION_CONTEXT_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_IMPL_SERIALIZATION_CONTEXT_HPP

#include <boost/mysql/impl/internal/protocol/frame_header.hpp>

#include <boost/assert.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/endian/conversion.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace boost {
namespace mysql {
namespace detail {

// Disables framing in serialization_context
BOOST_INLINE_CONSTEXPR std::size_t disable_framing = static_cast<std::size_t>(-1);

// Helper to compose a packet with any required frame headers. Embedding knowledge
// of frame headers in serialization functions creates messages ready to send.
// We require the entire message to be created before it's sent, so we don't lose any functionality.
//
// This class knows the offset of the next frame header. Adding data with add_checked will correctly
// insert space for headers as required while copying the data. Other functions may result in
// "overruns" (writing past the offset of the next header). Overruns are fixed by add_frame_headers,
// which will memmove data as required. The distinction is made for efficiency.
class serialization_context
{
    std::vector<std::uint8_t>& buffer_;
    std::size_t initial_offset_;
    std::size_t max_frame_size_;
    std::size_t next_header_offset_{};

    // max_frame_size_ == -1 can be used to disable framing. Used for testing
    bool framing_enabled() const { return max_frame_size_ != disable_framing; }

    void add_checked_impl(span<const std::uint8_t> content)
    {
        // Add any required frame headers we didn't add until now
        add_frame_headers();

        // Add the content in chunks, inserting space for headers where required
        std::size_t content_offset = 0;
        while (content_offset < content.size())
        {
            // Serialize what we've got space for
            BOOST_ASSERT(next_header_offset_ > buffer_.size());
            auto remaining_content = static_cast<std::size_t>(content.size() - content_offset);
            auto remaining_frame = static_cast<std::size_t>(next_header_offset_ - buffer_.size());
            auto size_to_write = (std::min)(remaining_content, remaining_frame);
            buffer_.insert(
                buffer_.end(),
                content.data() + content_offset,
                content.data() + content_offset + size_to_write
            );
            content_offset += size_to_write;

            // Insert space for a frame header if required
            if (buffer_.size() == next_header_offset_)
            {
                buffer_.resize(buffer_.size() + 4);
                next_header_offset_ += (max_frame_size_ + frame_header_size);
            }
        }
    }

public:
    serialization_context(std::vector<std::uint8_t>& buff, std::size_t max_frame_size = max_packet_size)
        : buffer_(buff), initial_offset_(buffer_.size()), max_frame_size_(max_frame_size)
    {
        // Add space for the initial header
        if (framing_enabled())
        {
            buffer_.resize(buffer_.size() + frame_header_size);
            next_header_offset_ = initial_offset_ + max_frame_size_ + frame_header_size;
        }
    }

    // Exposed for testing
    std::size_t next_header_offset() const { return next_header_offset_; }

    // To be called by serialize() functions.
    // Appends a single byte to the buffer. Doesn't take framing into account.
    void add(std::uint8_t value) { buffer_.push_back(value); }

    // To be called by serialize() functions. Appends bytes to the buffer.
    // Doesn't take framing into account - use for payloads with bound size.
    void add(span<const std::uint8_t> content)
    {
        buffer_.insert(buffer_.end(), content.begin(), content.end());
    }

    // Like add, but takes framing into account. Use for potentially long payloads.
    // If the payload is very long, space for frame headers will be added as required,
    // avoiding expensive memmove's when calling add_frame_headers
    void add_checked(span<const std::uint8_t> content)
    {
        if (framing_enabled())
        {
            add_checked_impl(content);
        }
        else
        {
            add(content);
        }
    }

    // Inserts any missing space for frame headers, moving data as required.
    // Exposed for testing
    void add_frame_headers()
    {
        while (next_header_offset_ <= buffer_.size())
        {
            // Insert space for the frame header where needed
            const std::array<std::uint8_t, frame_header_size> placeholder{};
            buffer_.insert(buffer_.begin() + next_header_offset_, placeholder.begin(), placeholder.end());

            // Update the next frame header offset
            next_header_offset_ += (max_frame_size_ + frame_header_size);
        }
    }

    // Write frame headers to an already serialized message with space for them
    std::uint8_t write_frame_headers(std::uint8_t seqnum)
    {
        BOOST_ASSERT(framing_enabled());

        // Add any missing space for headers
        add_frame_headers();

        // Actually write the headers
        std::size_t offset = initial_offset_;
        while (offset < buffer_.size())
        {
            // Calculate the current frame size
            std::size_t frame_first = offset + frame_header_size;
            std::size_t frame_last = (std::min)(frame_first + max_frame_size_, buffer_.size());
            auto frame_size = static_cast<std::uint32_t>(frame_last - frame_first);

            // Write the frame header
            BOOST_ASSERT(frame_first <= buffer_.size());
            serialize_frame_header(
                span<std::uint8_t, frame_header_size>(buffer_.data() + offset, frame_header_size),
                frame_header{frame_size, seqnum++}
            );

            // Skip to the next frame
            offset = frame_last;
        }

        // We should have finished just at the buffer end
        BOOST_ASSERT(offset == buffer_.size());

        return seqnum;
    }

    // Allow chaining
    template <class... Serializable>
    void serialize(Serializable... s)
    {
        int dummy[] = {(s.serialize(*this), 0)...};
        ignore_unused(dummy);
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
