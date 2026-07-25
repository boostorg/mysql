//
// Copyright (c) 2026 Vladislav Soulgard (vsoulgard at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/impl/internal/next_power_of_two.hpp>

#include <boost/test/unit_test.hpp>

#include <cstdint>

using namespace boost::mysql::detail;

BOOST_AUTO_TEST_SUITE(test_next_power_of_two)

BOOST_AUTO_TEST_CASE(basic)
{
    // n = 0 (special case)
    BOOST_TEST(next_power_of_two(0u) == 1u);

    // n is already power of two
    BOOST_TEST(next_power_of_two(1u) == 1u);
    BOOST_TEST(next_power_of_two(2u) == 2u);
    BOOST_TEST(next_power_of_two(4u) == 4u);
    BOOST_TEST(next_power_of_two(8u) == 8u);
    BOOST_TEST(next_power_of_two(16u) == 16u);
    BOOST_TEST(next_power_of_two(32u) == 32u);
    BOOST_TEST(next_power_of_two(64u) == 64u);
    BOOST_TEST(next_power_of_two(128u) == 128u);

    // n just below power of two
    BOOST_TEST(next_power_of_two(3u) == 4u);
    BOOST_TEST(next_power_of_two(7u) == 8u);
    BOOST_TEST(next_power_of_two(15u) == 16u);
    BOOST_TEST(next_power_of_two(31u) == 32u);
    BOOST_TEST(next_power_of_two(63u) == 64u);
    BOOST_TEST(next_power_of_two(127u) == 128u);

    // n just above power of two
    BOOST_TEST(next_power_of_two(5u) == 8u);
    BOOST_TEST(next_power_of_two(9u) == 16u);
    BOOST_TEST(next_power_of_two(17u) == 32u);
    BOOST_TEST(next_power_of_two(33u) == 64u);
    BOOST_TEST(next_power_of_two(65u) == 128u);
    BOOST_TEST(next_power_of_two(129u) == 256u);

    // n is random value
    BOOST_TEST(next_power_of_two(6u) == 8u);
    BOOST_TEST(next_power_of_two(13u) == 16u);
    BOOST_TEST(next_power_of_two(21u) == 32u);
    BOOST_TEST(next_power_of_two(45u) == 64u);
    BOOST_TEST(next_power_of_two(89u) == 128u);
    BOOST_TEST(next_power_of_two(200u) == 256u);
    BOOST_TEST(next_power_of_two(300u) == 512u);
    BOOST_TEST(next_power_of_two(400u) == 512u);
    BOOST_TEST(next_power_of_two(505u) == 512u);
    BOOST_TEST(next_power_of_two(888u) == 1024u);
}

BOOST_AUTO_TEST_SUITE_END()
