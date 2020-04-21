//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE_BOOST_MYSQL_DETAIL_AUXILIAR_STATIC_STRING_HPP_
#define INCLUDE_BOOST_MYSQL_DETAIL_AUXILIAR_STATIC_STRING_HPP_

// A very simplified variable-length string with fixed max-size

#include <array>
#include <string_view>
#include <cassert>
#include <cstring>
#include <ostream>

namespace boost {
namespace mysql {
namespace detail {

template <std::size_t max_size>
class static_string
{
    std::array<char, max_size> buffer_;
    std::size_t size_;

    void size_check(std::size_t sz) const noexcept { assert(sz <= max_size); }
public:
    static_string() noexcept: size_(0) {}
    static_string(std::string_view value) noexcept : size_(value.size())
    {
        size_check(value.size());
        size_ = value.size();
        std::memcpy(buffer_.data(), value.data(), value.size());
    }
    std::size_t size() const noexcept { return size_; }
    std::string_view value() const noexcept { return std::string_view(buffer_.data(), size_); }
    void append(const void* arr, std::size_t sz) noexcept
    {
        std::size_t new_size = size_ + sz;
        size_check(new_size);
        std::memcpy(buffer_.data() + size_, arr, sz);
        size_ = new_size;
    }
    void clear() noexcept { size_ = 0; }
    bool operator==(const static_string<max_size>& other) const noexcept
    {
        return value() == other.value();
    }
    bool operator!=(const static_string<max_size>& other) const noexcept
    {
        return !(*this == other);
    }
};

template <std::size_t max_size>
std::ostream& operator<<(std::ostream& os, const static_string<max_size>& value)
{
    return os << value.value();
}

} // detail
} // mysql
} // boost



#endif /* INCLUDE_BOOST_MYSQL_DETAIL_AUXILIAR_STATIC_STRING_HPP_ */
