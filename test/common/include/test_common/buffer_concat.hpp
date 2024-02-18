//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_COMMON_INCLUDE_TEST_COMMON_BUFFER_CONCAT_HPP
#define BOOST_MYSQL_TEST_COMMON_INCLUDE_TEST_COMMON_BUFFER_CONCAT_HPP

#include <boost/config.hpp>
#include <boost/core/span.hpp>

#include <cstdint>
#include <cstring>
#include <vector>

namespace boost {
namespace mysql {
namespace test {

// ARM gcc raises a spurious warning here
#if BOOST_GCC >= 110000
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#pragma GCC diagnostic ignored "-Wrestrict"
#endif
inline void concat(std::vector<std::uint8_t>& lhs, span<const std::uint8_t> rhs)
{
    if (!rhs.empty())
    {
        auto current_size = lhs.size();
        lhs.resize(current_size + rhs.size());
        std::memcpy(lhs.data() + current_size, rhs.data(), rhs.size());
    }
}
#if BOOST_GCC >= 110000
#pragma GCC diagnostic pop
#endif

inline std::vector<std::uint8_t> concat_copy(
    std::vector<std::uint8_t> lhs,
    const std::vector<std::uint8_t>& rhs
)
{
    concat(lhs, rhs);
    return lhs;
}

class buffer_builder
{
    std::vector<std::uint8_t> buff_;

public:
    buffer_builder() = default;
    buffer_builder& add(span<const std::uint8_t> value)
    {
        concat(buff_, value);
        return *this;
    }
    buffer_builder& add(const std::vector<std::uint8_t>& value)
    {
        return add(span<const std::uint8_t>(value));
    }
    std::vector<std::uint8_t> build() { return std::move(buff_); }
};

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
