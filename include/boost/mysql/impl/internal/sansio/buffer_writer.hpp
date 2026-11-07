//
// Copyright (c) 2026 Vladislav Soulgard (vsoulgard at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_BUFFER_WRITER_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_BUFFER_WRITER_HPP

#include <boost/mysql/impl/internal/next_power_of_two.hpp>

#include <boost/assert.hpp>
#include <boost/core/span.hpp>

#include <cstddef>
#include <cstring>
#include <vector>

namespace boost {
namespace mysql {
namespace detail {

class buffer_writer
{
    std::vector<std::uint8_t>& buffer_;
    std::size_t offset_{0};

public:
    buffer_writer() = delete;
    buffer_writer(std::vector<std::uint8_t>& buffer)
     : buffer_(buffer), offset_(buffer_.size()) {}

    inline std::uint8_t* data() noexcept { return buffer_.data(); }
    inline std::uint8_t* end() noexcept { return buffer_.data() + offset_; }
    inline std::size_t size() const noexcept { return offset_; }
    inline void clear() noexcept { offset_ = 0; }

    inline void resize(std::size_t n)
    {
        buffer_.resize(next_power_of_two(n));
        offset_ = n;
    }

    inline void insert(const std::uint8_t* pos, const std::uint8_t* first, const std::uint8_t* last)
    {
        BOOST_ASSERT(first < last);
        std::size_t insert_idx = pos - data();
        std::size_t count = std::distance(first, last);
        std::size_t new_offset = offset_ + count;
        if (new_offset > buffer_.size())
        {
            buffer_.resize(new_offset);
        }
        std::uint8_t* write_pos = data() + insert_idx;
        std::memcpy(write_pos, first, count);

        offset_ = new_offset;
    }

    inline void push_back(std::uint8_t x)
    {
        std::memcpy(data(), &x, 1u);
        offset_++;
    }

    operator span<const std::uint8_t> () const noexcept
    {
        return span<const std::uint8_t>{buffer_.data(), size()};
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
