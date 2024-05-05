//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_IMPL_NULL_BITMAP_TRAITS_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_IMPL_NULL_BITMAP_TRAITS_HPP

#include <boost/mysql/field_view.hpp>

#include <boost/assert.hpp>
#include <boost/config.hpp>
#include <boost/core/span.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace boost {
namespace mysql {
namespace detail {

class null_bitmap_traits
{
    std::size_t offset_;
    std::size_t num_fields_;

    constexpr std::size_t byte_pos(std::size_t field_pos) const noexcept { return (field_pos + offset_) / 8; }
    constexpr std::size_t bit_pos(std::size_t field_pos) const noexcept { return (field_pos + offset_) % 8; }

public:
    constexpr null_bitmap_traits(std::size_t offset, std::size_t num_fields) noexcept
        : offset_(offset), num_fields_{num_fields} {};
    constexpr std::size_t offset() const noexcept { return offset_; }
    constexpr std::size_t num_fields() const noexcept { return num_fields_; }

    constexpr std::size_t byte_count() const noexcept { return (num_fields_ + 7 + offset_) / 8; }
    bool is_null(const std::uint8_t* null_bitmap_begin, std::size_t field_pos) const noexcept
    {
        BOOST_ASSERT(field_pos < num_fields_);
        return null_bitmap_begin[byte_pos(field_pos)] & (1 << bit_pos(field_pos));
    }
};

class null_bitmap_generator
{
    span<const field_view> fields_;
    std::size_t current_{0};

public:
    null_bitmap_generator(span<const field_view> fields) noexcept : fields_(fields) {}
    bool done() const { return current_ == fields_.size(); }
    std::uint8_t next()
    {
        BOOST_ASSERT(current_ < fields_.size());

        std::uint8_t res = 0;

        // Generate
        const std::size_t max_i = (std::min)(fields_.size(), current_ + 8u);
        for (std::size_t i = current_; i < max_i; ++i)
        {
            if (fields_[i].is_null())
            {
                const auto bit_pos = i % 8;
                res |= (1 << bit_pos);
            }
        }

        // Update state
        current_ = max_i;

        // Return
        return res;
    }
};

BOOST_INLINE_CONSTEXPR std::size_t stmt_execute_null_bitmap_offset = 0;
BOOST_INLINE_CONSTEXPR std::size_t binary_row_null_bitmap_offset = 2;

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif /* INCLUDE_NULL_BITMAP_HPP_ */
