//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_MESSAGE_WRITER_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_MESSAGE_WRITER_HPP

#include <boost/mysql/impl/internal/protocol/constants.hpp>
#include <boost/mysql/impl/internal/protocol/protocol.hpp>

#include <cstddef>
#include <cstdint>

namespace boost {
namespace mysql {
namespace detail {

class message_writer
{
    std::vector<std::uint8_t> buffer_;
    std::size_t offset_{0};

    void reset()
    {
        buffer_.clear();
        offset_ = 0;
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

    bool done() const noexcept { return offset_ == buffer_.size(); }

    span<const std::uint8_t> current_chunk() const
    {
        BOOST_ASSERT(!done());
        BOOST_ASSERT(!buffer_.empty());
        return span<const std::uint8_t>(buffer_).subspan(offset_);
    }

    void resume(std::size_t n)
    {
        std::size_t remaining = static_cast<std::size_t>(buffer_.size() - offset_);
        BOOST_ASSERT(n <= remaining);
        offset_ += n;
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
