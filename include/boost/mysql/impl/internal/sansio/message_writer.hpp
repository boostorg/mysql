//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_MESSAGE_WRITER_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_MESSAGE_WRITER_HPP

#include <boost/assert.hpp>
#include <boost/core/span.hpp>

#include <cstddef>
#include <cstdint>

namespace boost {
namespace mysql {
namespace detail {

class message_writer
{
    span<const std::uint8_t> buffer_;

public:
    message_writer() = default;

    void reset(span<const std::uint8_t> buffer_to_write) { buffer_ = buffer_to_write; }

    bool done() const { return buffer_.empty(); }

    span<const std::uint8_t> current_chunk() const { return buffer_; }

    void resume(std::size_t n)
    {
        BOOST_ASSERT(n <= buffer_.size());
        buffer_ = buffer_.subspan(n);
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
