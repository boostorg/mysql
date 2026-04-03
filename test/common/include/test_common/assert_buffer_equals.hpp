//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_COMMON_INCLUDE_TEST_COMMON_ASSERT_BUFFER_EQUALS_HPP
#define BOOST_MYSQL_TEST_COMMON_INCLUDE_TEST_COMMON_ASSERT_BUFFER_EQUALS_HPP

#include <boost/core/lightweight_test.hpp>

#include <span>

namespace boost {
namespace mysql {
namespace test {

bool buffer_equals(std::span<const std::uint8_t> b1, std::span<const std::uint8_t> b2);

struct buffer_printer
{
    std::span<const std::uint8_t> buff;

    constexpr buffer_printer(std::span<const std::uint8_t> b) noexcept : buff(b) {}
};

std::ostream& operator<<(std::ostream& os, buffer_printer buff);
inline bool operator==(buffer_printer b1, buffer_printer b2) { return buffer_equals(b1.buff, b2.buff); }

}  // namespace test
}  // namespace mysql
}  // namespace boost

#define BOOST_MYSQL_ASSERT_BUFFER_EQUALS(b1, b2) BOOST_TEST_EQ(buffer_printer{b1}, buffer_printer{b2})

#endif
