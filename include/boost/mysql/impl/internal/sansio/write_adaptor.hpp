//
// Copyright (c) 2026 Vladislav Soulgard (vsoulgard at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_WRITE_ADAPTOR_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_WRITE_ADAPTOR_HPP

#include <boost/mysql/impl/internal/next_power_of_two.hpp>
#include <boost/mysql/impl/internal/sansio/rw_buffer.hpp>

#include <boost/assert.hpp>
#include <boost/core/span.hpp>

#include <cstddef>
#include <cstring>
#include <iterator>
#include <vector>

namespace boost {
namespace mysql {
namespace detail {

class write_adaptor
{
    rw_buffer& buffer_;
    std::size_t offset_{0};

public:
    write_adaptor() = delete;
    explicit write_adaptor(rw_buffer& buffer)
     : buffer_(buffer) {}

    inline std::uint8_t* data() noexcept { return buffer_.free_first(); }
    inline std::uint8_t* end() noexcept { return buffer_.free_first() + offset_; }
    inline std::size_t size() const noexcept { return offset_; }
    inline void clear() noexcept { offset_ = 0; }

    inline void insert(std::uint8_t* pos, const std::uint8_t* first, const std::uint8_t* last)
    {
        BOOST_ASSERT(first < last);
        std::size_t count = std::distance(first, last);
        std::size_t new_offset = offset_ + count;
        std::size_t insert_idx = pos - data();
        auto ec = buffer_.grow_to_fit(new_offset);
        BOOST_ASSERT(!ec);
        (void)ec;
        if (insert_idx < offset_)
        {
            std::memmove(data() + insert_idx + count, data() + insert_idx, offset_ - insert_idx);
        }
        std::memcpy(data() + insert_idx, first, count);
        offset_ = new_offset;
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
