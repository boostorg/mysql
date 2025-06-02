//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/client_errc.hpp>

#include <boost/mysql/impl/internal/sansio/mysql_native_password.hpp>

#include "test_common/assert_buffer_equals.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;
using detail::mnp_hash_password;

namespace {

BOOST_AUTO_TEST_SUITE(test_handshake_mnp_hash_password)

// Values snooped using Wireshark
constexpr std::uint8_t scramble[20] = {
    0x79, 0x64, 0x3d, 0x12, 0x1d, 0x71, 0x74, 0x47, 0x5f, 0x48,
    0x3e, 0x3e, 0x0b, 0x62, 0x0a, 0x03, 0x3d, 0x27, 0x3a, 0x4c,
};

BOOST_AUTO_TEST_CASE(nonempty_password)
{
    constexpr std::uint8_t expected_hash[20] = {
        0xf1, 0xb2, 0xfb, 0x1c, 0x8d, 0xe7, 0x5d, 0xb8, 0xeb, 0xa8,
        0x12, 0x6a, 0xd1, 0x0f, 0xe9, 0xb1, 0x10, 0x50, 0xd4, 0x28,
    };

    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(mnp_hash_password("root", scramble), expected_hash);
}

BOOST_AUTO_TEST_CASE(empty_password)
{
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(mnp_hash_password("", scramble), std::vector<std::uint8_t>());
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace
