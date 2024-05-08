//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_MESSAGE_WRITER_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_MESSAGE_WRITER_HPP

#include <boost/mysql/impl/internal/protocol/serialization.hpp>

#include <boost/assert.hpp>
#include <boost/core/ignore_unused.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace boost {
namespace mysql {
namespace detail {

class message_writer
{
    // TODO: this is a horrible workaround
    const std::vector<std::uint8_t>* pipeline_buffer_{};
    std::vector<std::uint8_t> buffer_;
    std::size_t offset_{0};

    void reset()
    {
        pipeline_buffer_ = nullptr;
        buffer_.clear();
        offset_ = 0;
    }

    const std::vector<std::uint8_t>& get_buffer() const
    {
        return pipeline_buffer_ ? *pipeline_buffer_ : buffer_;
    }

public:
    message_writer() = default;

    template <class Serializable>
    void prepare_write(const Serializable& message, std::uint8_t& sequence_number)
    {
        reset();
        sequence_number = serialize_top_level(message, buffer_, sequence_number);
        resume(0);
    }

    // Serializes two messages into the write buffer. They must fit in a single frame
    template <class Serializable1, class Serializable2>
    void prepare_pipelined_write(
        const Serializable1& msg1,
        std::uint8_t& seqnum1,
        const Serializable2& msg2,
        std::uint8_t& seqnum2
    )
    {
        reset();
        seqnum1 = serialize_top_level(msg1, buffer_, seqnum1);
        seqnum2 = serialize_top_level(msg2, buffer_, seqnum2);
        resume(0);
    }

    void prepare_pipelined_write(const std::vector<std::uint8_t>& pipeline_buffer)
    {
        pipeline_buffer_ = &pipeline_buffer;
        offset_ = 0u;
    }

    bool done() const noexcept { return offset_ == get_buffer().size(); }

    span<const std::uint8_t> current_chunk() const
    {
        BOOST_ASSERT(!done());
        BOOST_ASSERT(!get_buffer().empty());
        return span<const std::uint8_t>(get_buffer()).subspan(offset_);
    }

    void resume(std::size_t n)
    {
        std::size_t remaining = static_cast<std::size_t>(get_buffer().size() - offset_);
        BOOST_ASSERT(n <= remaining);
        ignore_unused(remaining);
        offset_ += n;
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
