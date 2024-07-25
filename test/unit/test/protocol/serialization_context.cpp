//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/string_view.hpp>

#include <boost/mysql/impl/internal/protocol/impl/serialization_context.hpp>

#include <boost/test/unit_test.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "test_common/assert_buffer_equals.hpp"
#include "test_common/buffer_concat.hpp"

using namespace boost::mysql;

namespace {

BOOST_AUTO_TEST_SUITE(test_serialization_context)

struct framing_test_case
{
    string_view name;
    std::size_t expected_next_frame_offset;  // not counting previous contents
    std::vector<std::uint8_t> payload;
    std::vector<std::uint8_t> expected_buffer;  // not counting previous contents
};

std::vector<framing_test_case> make_test_cases()
{
    return {
        // clang-format off
        {"0 bytes",     12,   {},                          {0, 0, 0, 0}                                       },
        {"1 byte",      12,   {1},                         {0, 0, 0, 0, 1}                                    },
        {"5 bytes",     12,   {1, 2, 3, 4, 5},             {0, 0, 0, 0, 1, 2, 3, 4, 5}                        },
        {"fs-1 bytes",  12,   {1, 2, 3, 4, 5, 6, 7},       {0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7}                  },
        {"fs bytes",    24,   {1, 2, 3, 4, 5, 6, 7, 8},    {0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 0, 0, 0, 0}   },
        {"fs+1 bytes",  24,   {1, 2, 3, 4, 5, 6, 7, 8, 9}, {0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 0, 0, 0, 0, 9}},
        {"2fs bytes",   36,   {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16},
            {0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 0, 0, 0, 0, 9, 10, 11, 12, 13, 14, 15, 16, 0, 0, 0, 0}    },
        {"2fs+1 bytes", 36,   {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17},
            {0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 0, 0, 0, 0, 9, 10, 11, 12, 13, 14, 15, 16, 0, 0, 0, 0, 17}},
        {"2fs+5 bytes", 36,   {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21},
            {0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 0, 0, 0, 0, 9, 10, 11, 12, 13, 14, 15, 16, 0, 0, 0, 0, 17, 18, 19, 20, 21}},
        {"3fs-1 bytes", 36,   {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23},
            {0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 0, 0, 0, 0, 9, 10, 11, 12, 13, 14, 15, 16, 0, 0, 0, 0, 17, 18, 19, 20, 21, 22, 23}},
        {"3fs bytes",   48,   {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24},
            {0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 0, 0, 0, 0, 9, 10, 11, 12, 13, 14, 15, 16, 0, 0, 0, 0, 17, 18, 19, 20, 21, 22, 23, 24, 0, 0, 0, 0}},
        {"3fs+1 bytes", 48,   {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25},
            {0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 0, 0, 0, 0, 9, 10, 11, 12, 13, 14, 15, 16, 0, 0, 0, 0, 17, 18, 19, 20, 21, 22, 23, 24, 0, 0, 0, 0, 25}},
        // clang-format on
    };
}

BOOST_AUTO_TEST_CASE(add)
{
    constexpr std::size_t fs = 8u;  // frame size
    const std::vector<std::uint8_t> initial_buffer{0xaa, 0xbb, 0xcc, 0xdd, 0xee};

    for (const auto& tc : make_test_cases())
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            // Setup
            std::vector<std::uint8_t> buff{initial_buffer};
            detail::serialization_context ctx(buff, fs);

            // Add the payload
            ctx.add(tc.payload);

            // Check
            auto expected = test::concat_copy(initial_buffer, tc.expected_buffer);
            BOOST_MYSQL_ASSERT_BUFFER_EQUALS(buff, expected);
            BOOST_TEST(ctx.next_header_offset() == tc.expected_next_frame_offset + initial_buffer.size());
        }
    }
}

// Spotcheck: if the initial buffer is empty, everything works fine
BOOST_AUTO_TEST_CASE(add_initial_buffer_empty)
{
    // Setup
    std::vector<std::uint8_t> buff;
    detail::serialization_context ctx(buff, 8);

    // Add data
    const std::array<std::uint8_t, 10> payload{
        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}
    };
    ctx.add(payload);

    // Check
    const std::vector<std::uint8_t> expected{0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 0, 0, 0, 0, 9, 10};
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(buff, expected);
    BOOST_TEST(ctx.next_header_offset() == 24u);
}

// Spotcheck: adding single bytes or in chunks also works fine
BOOST_AUTO_TEST_CASE(chunks)
{
    // Setup
    std::vector<std::uint8_t> buff;
    detail::serialization_context ctx(buff, 8);
    const std::array<std::uint8_t, 4> payload1{
        {1, 2, 3, 4}
    };
    const std::array<std::uint8_t, 5> payload2{
        {5, 6, 7, 8, 9}
    };

    // Add byte
    ctx.add(0xff);
    std::vector<std::uint8_t> expected{0, 0, 0, 0, 0xff};
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(buff, expected);

    // Add buffer
    ctx.add(payload1);
    expected = {0, 0, 0, 0, 0xff, 1, 2, 3, 4};
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(buff, expected);

    // Add byte
    ctx.add(0xfe);
    expected = {0, 0, 0, 0, 0xff, 1, 2, 3, 4, 0xfe};
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(buff, expected);

    // Add buffer
    ctx.add(payload2);
    expected = {0, 0, 0, 0, 0xff, 1, 2, 3, 4, 0xfe, 5, 6, 0, 0, 0, 0, 7, 8, 9};
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(buff, expected);

    // Add byte
    ctx.add(0xfc);
    expected = {0, 0, 0, 0, 0xff, 1, 2, 3, 4, 0xfe, 5, 6, 0, 0, 0, 0, 7, 8, 9, 0xfc};
    BOOST_TEST(ctx.next_header_offset() == 24u);
}

// Spotcheck: adding a single byte that causes a frame header to be written works
BOOST_AUTO_TEST_CASE(add_byte_fills_frame)
{
    // Setup
    std::vector<std::uint8_t> buff;
    detail::serialization_context ctx(buff, 8);
    const std::array<std::uint8_t, 7> payload{
        {1, 2, 3, 4, 5, 6, 7}
    };

    // Add payload
    ctx.add(payload);
    std::vector<std::uint8_t> expected{0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7};
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(buff, expected);
    BOOST_TEST(ctx.next_header_offset() == 12u);

    // Add byte
    ctx.add(0xab);
    expected = {0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 0xab, 0, 0, 0, 0};
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(buff, expected);
    BOOST_TEST(ctx.next_header_offset() == 24u);
}

BOOST_AUTO_TEST_CASE(write_frame_headers)
{
    struct
    {
        string_view name;
        std::uint8_t expected_seqnum;
        std::vector<std::uint8_t> payload;
        std::vector<std::uint8_t> expected;
    } test_cases[] = {
        // clang-format off
        {"0 bytes",     43,   {},                          {0, 0, 0, 42}                                       },
        {"1 byte",      43,   {1},                         {1, 0, 0, 42, 1}                                    },
        {"5 bytes",     43,   {1, 2, 3, 4, 5},             {5, 0, 0, 42, 1, 2, 3, 4, 5}                        },
        {"fs-1 bytes",  43,   {1, 2, 3, 4, 5, 6, 7},       {7, 0, 0, 42, 1, 2, 3, 4, 5, 6, 7}                  },
        {"fs bytes",    44,   {1, 2, 3, 4, 5, 6, 7, 8},    {8, 0, 0, 42, 1, 2, 3, 4, 5, 6, 7, 8, 0, 0, 0, 43}   },
        {"fs+1 bytes",  44,   {1, 2, 3, 4, 5, 6, 7, 8, 9}, {8, 0, 0, 42, 1, 2, 3, 4, 5, 6, 7, 8, 1, 0, 0, 43, 9}},
        {"2fs bytes",   45,   {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16},
            {8, 0, 0, 42, 1, 2, 3, 4, 5, 6, 7, 8, 8, 0, 0, 43, 9, 10, 11, 12, 13, 14, 15, 16, 0, 0, 0, 44}    },
        {"2fs+1 bytes", 45,   {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17},
            {8, 0, 0, 42, 1, 2, 3, 4, 5, 6, 7, 8, 8, 0, 0, 43, 9, 10, 11, 12, 13, 14, 15, 16, 1, 0, 0, 44, 17}},
        {"2fs+5 bytes", 45,   {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21},
            {8, 0, 0, 42, 1, 2, 3, 4, 5, 6, 7, 8, 8, 0, 0, 43, 9, 10, 11, 12, 13, 14, 15, 16, 5, 0, 0, 44, 17, 18, 19, 20, 21}},
        {"3fs-1 bytes", 45,   {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23},
            {8, 0, 0, 42, 1, 2, 3, 4, 5, 6, 7, 8, 8, 0, 0, 43, 9, 10, 11, 12, 13, 14, 15, 16, 7, 0, 0, 44, 17, 18, 19, 20, 21, 22, 23}},
        {"3fs bytes",   46,   {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24},
            {8, 0, 0, 42, 1, 2, 3, 4, 5, 6, 7, 8, 8, 0, 0, 43, 9, 10, 11, 12, 13, 14, 15, 16, 8, 0, 0, 44, 17, 18, 19, 20, 21, 22, 23, 24,
                0, 0, 0, 45}},
        {"3fs+1 bytes", 46,   {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25},
            {8, 0, 0, 42, 1, 2, 3, 4, 5, 6, 7, 8, 8, 0, 0, 43, 9, 10, 11, 12, 13, 14, 15, 16, 8, 0, 0, 44, 17, 18, 19, 20, 21, 22, 23, 24,
                1, 0, 0, 45, 25}},
        // clang-format on
    };

    const std::vector<std::uint8_t> initial_buffer{90, 91, 92, 93, 94};

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            // Setup
            std::vector<std::uint8_t> buff{initial_buffer};
            detail::serialization_context ctx(buff, 8);
            ctx.add(tc.payload);

            // Call and check
            auto seqnum = ctx.write_frame_headers(42);
            const auto expected = test::concat_copy(initial_buffer, tc.expected);
            BOOST_MYSQL_ASSERT_BUFFER_EQUALS(buff, expected);
            BOOST_TEST(seqnum == tc.expected_seqnum);
        }
    }
}

// Spotcheck: we correctly wrap sequence numbers when going over 0xff
BOOST_AUTO_TEST_CASE(write_frame_headers_seqnum_wrap)
{
    // Setup
    std::vector<std::uint8_t> buff;
    detail::serialization_context ctx(buff, 8);
    for (std::uint8_t i = 1; i <= 20; ++i)
        ctx.add(i);

    // Call and check
    const std::vector<std::uint8_t> expected{
        8, 0, 0, 0xfe, 1,  2,  3,  4,  5,  6,  7,  8,   // frame 1
        8, 0, 0, 0xff, 9,  10, 11, 12, 13, 14, 15, 16,  // frame 2
        4, 0, 0, 0,    17, 18, 19, 20                   // frame 3
    };
    auto seqnum = ctx.write_frame_headers(0xfe);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(buff, expected);
    BOOST_TEST(seqnum == 1u);
}

// Spotcheck: disable framing works
BOOST_AUTO_TEST_CASE(disable_framing)
{
    // Setup
    std::vector<std::uint8_t> buff;
    detail::serialization_context ctx(buff, detail::disable_framing);

    // Add data using the several functions available
    const std::array<std::uint8_t, 5> payload1{
        {1, 2, 3, 4, 5}
    };
    const std::array<std::uint8_t, 4> payload2{
        {6, 7, 8, 9}
    };
    ctx.add(42);
    ctx.add(payload1);
    ctx.add(payload2);

    // We didn't add any framing
    const std::vector<std::uint8_t> expected{42, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(buff, expected);
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace