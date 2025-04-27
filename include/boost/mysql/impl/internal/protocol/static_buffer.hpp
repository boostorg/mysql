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
#include <boost/core/span.hpp>

#include <array>
#include <cstddef>
#include <cstring>

namespace boost {
namespace mysql {
namespace detail {

template <std::size_t N>
class static_buffer
{
    std::array<std::uint8_t, N> buffer_{};
    std::size_t size_{};

public:
    static constexpr std::size_t max_size = N;

    static_buffer() = default;
    static_buffer(std::size_t sz) noexcept : size_(sz) { BOOST_ASSERT(sz <= size_); }

    span<const std::uint8_t> to_span() const { return {buffer_.data(), size_}; }
    const std::uint8_t* data() const { return buffer_.data(); }
    std::uint8_t* data() { return buffer_.data(); }
    void resize(std::size_t sz)
    {
        // TODO: test
        BOOST_ASSERT(sz <= N);
        size_ = sz;
    }
    void append(const void* data, std::size_t data_size)
    {
        std::size_t new_size = size_ + data_size;
        BOOST_ASSERT(new_size <= N);
        std::memcpy(buffer_.data() + size_, data, data_size);
        size_ = new_size;
    }
    void clear() { size_ = 0; }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
