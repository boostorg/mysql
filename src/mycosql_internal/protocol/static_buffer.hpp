//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_STATIC_BUFFER_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_STATIC_BUFFER_HPP

// A very simplified variable-length buffer with fixed max-size

#include <boost/assert.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace boost {
namespace mysql {
namespace detail {

template <std::size_t N>
class static_buffer
{
    std::array<std::uint8_t, N> buffer_{};
    std::size_t size_{};

public:
    // Allow inspecting the supplied template argument
    static constexpr std::size_t max_size = N;

    // Constructors
    static_buffer() = default;
    static_buffer(std::size_t sz) noexcept : size_(sz) { BOOST_ASSERT(sz <= size_); }

    // Iterators
    using iterator = std::uint8_t*;
    using const_iterator = const std::uint8_t*;

    iterator begin() noexcept { return buffer_.data(); }
    iterator end() noexcept { return buffer_.data() + size_; }
    const_iterator begin() const noexcept { return buffer_.data(); }
    const_iterator end() const noexcept { return buffer_.data() + size_; }

    // Size and data
    std::size_t size() const noexcept { return size_; }
    const std::uint8_t* data() const noexcept { return buffer_.data(); }
    std::uint8_t* data() noexcept { return buffer_.data(); }

    // Modifiers
    void append(std::span<const std::uint8_t> data) noexcept
    {
        if (!data.empty())
        {
            std::size_t new_size = size_ + data.size();
            BOOST_ASSERT(new_size <= N);
            std::memcpy(buffer_.data() + size_, data.data(), data.size());
            size_ = new_size;
        }
    }

    void clear() noexcept { size_ = 0; }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
