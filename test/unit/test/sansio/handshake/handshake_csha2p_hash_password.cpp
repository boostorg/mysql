//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/impl/internal/sansio/caching_sha2_password.hpp>

#include <boost/test/unit_test.hpp>

#include "test_common/assert_buffer_equals.hpp"

using namespace boost::mysql;
using detail::csha2p_hash_password;

namespace {

BOOST_AUTO_TEST_SUITE(test_handshake_csha2p_hash_password)

// Values snooped using Wireshark
constexpr std::uint8_t scramble[20] = {
    0x3e, 0x3b, 0x4,  0x55, 0x4,  0x70, 0x16, 0x3a, 0x4c, 0x15,
    0x35, 0x3,  0x15, 0x76, 0x73, 0x22, 0x46, 0x8,  0x18, 0x1,
};

BOOST_AUTO_TEST_CASE(nonempty_password)
{
    constexpr std::uint8_t expected_hash[32] = {
        0xa1, 0xc1, 0xe1, 0xe9, 0x1b, 0xb6, 0x54, 0x4b, 0xa7, 0x37, 0x4b, 0x9c, 0x56, 0x6d, 0x69, 0x3e,
        0x6,  0xca, 0x7,  0x2,  0x98, 0xac, 0xd1, 0x6,  0x18, 0xc6, 0x90, 0x38, 0x9d, 0x88, 0xe1, 0x20,
    };

    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(csha2p_hash_password("hola", scramble), expected_hash);
}

BOOST_AUTO_TEST_CASE(empty_password)
{
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(csha2p_hash_password("", scramble), std::vector<std::uint8_t>());
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace