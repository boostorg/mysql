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

#include "test_common/assert_buffer_equals.hpp"
#include "test_common/buffer_concat.hpp"

using namespace boost::mysql;

BOOST_AUTO_TEST_SUITE(test_serialization_context)

BOOST_AUTO_TEST_CASE(add_frame_headers)
{
    struct
    {
        string_view name;
        std::size_t expected_next_frame_offset;  // not counting previous contents
        std::vector<std::uint8_t> payload;
        std::vector<std::uint8_t> expected_buffer;  // not counting previous contents
    } test_cases[] = {
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

    constexpr std::size_t fs = 8u;  // frame size
    const std::vector<std::uint8_t> initial_buffer{0xaa, 0xbb, 0xcc, 0xdd, 0xee};

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            // Setup
            std::vector<std::uint8_t> buff{initial_buffer};
            detail::serialization_context ctx(buff, fs);

            // Add payload and set headers
            ctx.add(tc.payload);
            ctx.add_frame_headers();

            // Check
            auto expected = test::concat_copy(initial_buffer, tc.expected_buffer);
            BOOST_MYSQL_ASSERT_BUFFER_EQUALS(buff, expected);
            BOOST_TEST(ctx.next_header_offset() == tc.expected_next_frame_offset + initial_buffer.size());
        }
    }
}

// Spotcheck: if the initial buffer is empty, everything works fine
BOOST_AUTO_TEST_CASE(add_frame_headers_initial_buffer_empty)
{
    // Setup
    std::vector<std::uint8_t> buff;
    detail::serialization_context ctx(buff, 8);

    // Add data
    const std::array<std::uint8_t, 10> payload{
        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}
    };
    ctx.add(payload);
    ctx.add_frame_headers();

    // Check
    const std::vector<std::uint8_t> expected{0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 0, 0, 0, 0, 9, 10};
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(buff, expected);
    BOOST_TEST(ctx.next_header_offset() == 24u);
}

// framing disabled
//   add (u8)
//   add (span)
//   add (span past 0xffffff)
//   grow_by zeroes
// framing enabled
//   Aux: size cases
//      size 0
//      size 1
//      size 5
//      size F-1
//      size F
//      size F+1
//      size 2F-1
//      size 2F
//      size 2F+1
//      size 2F+5
//      size 3F
//  add (u8): spotcheck: should behave like add(span)
//  add in chunks: spotcheck
//  grow_by:  spotcheck: should behave like add(span)
//  add_checked: with all sizes
//               with some missing frame headers before
//  Ctor adds an initial frame
//  write_frame_headers: with all sizes
//                     seqnum wrap
//  frame header with more than 0xff

// TODO: serialize_top_level in serialization.cpp

BOOST_AUTO_TEST_SUITE_END()
