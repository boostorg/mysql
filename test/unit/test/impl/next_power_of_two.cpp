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
    BOOST_TEST(next_power_of_two<std::size_t>(0u) == 1u);
    
    // n is already power of two
    BOOST_TEST(next_power_of_two<std::size_t>(1u) == 1u);
    BOOST_TEST(next_power_of_two<std::size_t>(2u) == 2u);
    BOOST_TEST(next_power_of_two<std::size_t>(4u) == 4u);
    BOOST_TEST(next_power_of_two<std::size_t>(8u) == 8u);
    BOOST_TEST(next_power_of_two<std::size_t>(16u) == 16u);
    BOOST_TEST(next_power_of_two<std::size_t>(32u) == 32u);
    BOOST_TEST(next_power_of_two<std::size_t>(64u) == 64u);
    BOOST_TEST(next_power_of_two<std::size_t>(128u) == 128u);
    
    // n just below power of two
    BOOST_TEST(next_power_of_two<std::size_t>(3u) == 4u);
    BOOST_TEST(next_power_of_two<std::size_t>(7u) == 8u);
    BOOST_TEST(next_power_of_two<std::size_t>(15u) == 16u);
    BOOST_TEST(next_power_of_two<std::size_t>(31u) == 32u);
    BOOST_TEST(next_power_of_two<std::size_t>(63u) == 64u);
    BOOST_TEST(next_power_of_two<std::size_t>(127u) == 128u);
    
    // n just above power of two
    BOOST_TEST(next_power_of_two<std::size_t>(5u) == 8u);
    BOOST_TEST(next_power_of_two<std::size_t>(9u) == 16u);
    BOOST_TEST(next_power_of_two<std::size_t>(17u) == 32u);
    BOOST_TEST(next_power_of_two<std::size_t>(33u) == 64u);
    BOOST_TEST(next_power_of_two<std::size_t>(65u) == 128u);
    BOOST_TEST(next_power_of_two<std::size_t>(129u) == 256u);
}

BOOST_AUTO_TEST_CASE(different_types)
{
    // uint8_t
    BOOST_TEST(next_power_of_two<std::uint8_t>(0u) == 1u);
    BOOST_TEST(next_power_of_two<std::uint8_t>(62u) == 64u);
    BOOST_TEST(next_power_of_two<std::uint8_t>(100u) == 128u);
    BOOST_TEST(next_power_of_two<std::uint8_t>(128u) == 128u);
    
    // uint16_t
    BOOST_TEST(next_power_of_two<std::uint16_t>(0u) == 1u);
    BOOST_TEST(next_power_of_two<std::uint16_t>(1000u) == 1024u);
    BOOST_TEST(next_power_of_two<std::uint16_t>(16383u) == 16384u);
    BOOST_TEST(next_power_of_two<std::uint16_t>(32768u) == 32768u);
    
    // uint32_t
    BOOST_TEST(next_power_of_two<std::uint32_t>(0u) == 1u);
    BOOST_TEST(next_power_of_two<std::uint32_t>(100000) == 131072u);
    BOOST_TEST(next_power_of_two<std::uint32_t>(1u << 30) == 1u << 30);
    BOOST_TEST(next_power_of_two<std::uint32_t>((1u << 30) + 1) == 1u << 31);
    
    // uint64_t
    BOOST_TEST(next_power_of_two<std::uint64_t>(0u) == 1u);
    BOOST_TEST(next_power_of_two<std::uint64_t>(1ull << 40) == 1ull << 40);
    BOOST_TEST(next_power_of_two<std::uint64_t>((1ull << 40) + 1) == 1ull << 41);
    BOOST_TEST(next_power_of_two<std::uint64_t>(1ull << 62) == 1ull << 62);
    BOOST_TEST(next_power_of_two<std::uint64_t>((1ull << 62) + 1) == 1ull << 63);
}

BOOST_AUTO_TEST_SUITE_END()
