//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/field_view.hpp>

#include <boost/mysql/impl/internal/protocol/impl/null_bitmap_traits.hpp>

#include <boost/core/span.hpp>
#include <boost/test/data/monomorphic/collection.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>

#include <array>
#include <cstdint>
#include <vector>

#include "test_common/assert_buffer_equals.hpp"
#include "test_common/create_basic.hpp"

using namespace boost::mysql::detail;
using namespace boost::unit_test;
using namespace boost::mysql::test;
using boost::mysql::field_view;

BOOST_AUTO_TEST_SUITE(test_null_bitmap_traits)

// byte_count
struct byte_count_sample
{
    std::size_t offset;
    std::size_t num_fields;
    std::size_t expected_value;
};

std::ostream& operator<<(std::ostream& os, const byte_count_sample& val)
{
    return os << "(offset=" << val.offset << ", num_fields=" << val.num_fields << ")";
}

constexpr byte_count_sample all_byte_count_samples[]{
    {stmt_execute_null_bitmap_offset, 0,  0},
    {stmt_execute_null_bitmap_offset, 1,  1},
    {stmt_execute_null_bitmap_offset, 2,  1},
    {stmt_execute_null_bitmap_offset, 3,  1},
    {stmt_execute_null_bitmap_offset, 4,  1},
    {stmt_execute_null_bitmap_offset, 5,  1},
    {stmt_execute_null_bitmap_offset, 6,  1},
    {stmt_execute_null_bitmap_offset, 7,  1},
    {stmt_execute_null_bitmap_offset, 8,  1},
    {stmt_execute_null_bitmap_offset, 9,  2},
    {stmt_execute_null_bitmap_offset, 10, 2},
    {stmt_execute_null_bitmap_offset, 11, 2},
    {stmt_execute_null_bitmap_offset, 12, 2},
    {stmt_execute_null_bitmap_offset, 13, 2},
    {stmt_execute_null_bitmap_offset, 14, 2},
    {stmt_execute_null_bitmap_offset, 15, 2},
    {stmt_execute_null_bitmap_offset, 16, 2},
    {stmt_execute_null_bitmap_offset, 17, 3},

    {binary_row_null_bitmap_offset,   0,  1},
    {binary_row_null_bitmap_offset,   1,  1},
    {binary_row_null_bitmap_offset,   2,  1},
    {binary_row_null_bitmap_offset,   3,  1},
    {binary_row_null_bitmap_offset,   4,  1},
    {binary_row_null_bitmap_offset,   5,  1},
    {binary_row_null_bitmap_offset,   6,  1},
    {binary_row_null_bitmap_offset,   7,  2},
    {binary_row_null_bitmap_offset,   8,  2},
    {binary_row_null_bitmap_offset,   9,  2},
    {binary_row_null_bitmap_offset,   10, 2},
    {binary_row_null_bitmap_offset,   11, 2},
    {binary_row_null_bitmap_offset,   12, 2},
    {binary_row_null_bitmap_offset,   13, 2},
    {binary_row_null_bitmap_offset,   14, 2},
    {binary_row_null_bitmap_offset,   15, 3},
    {binary_row_null_bitmap_offset,   16, 3},
    {binary_row_null_bitmap_offset,   17, 3},
};

BOOST_DATA_TEST_CASE(byte_count, data::make(all_byte_count_samples))
{
    null_bitmap_traits traits(sample.offset, sample.num_fields);
    BOOST_TEST(traits.byte_count() == sample.expected_value);
}

// is_null
struct is_null_sample
{
    std::size_t offset;
    std::size_t pos;
    bool expected;
};

std::ostream& operator<<(std::ostream& os, const is_null_sample& value)
{
    return os << "(offset=" << value.offset << ", pos=" << value.pos << ")";
}

constexpr is_null_sample all_is_null_samples[]{
    {stmt_execute_null_bitmap_offset, 0,  false},
    {stmt_execute_null_bitmap_offset, 1,  false},
    {stmt_execute_null_bitmap_offset, 2,  true },
    {stmt_execute_null_bitmap_offset, 3,  false},
    {stmt_execute_null_bitmap_offset, 4,  true },
    {stmt_execute_null_bitmap_offset, 5,  true },
    {stmt_execute_null_bitmap_offset, 6,  false},
    {stmt_execute_null_bitmap_offset, 7,  true },
    {stmt_execute_null_bitmap_offset, 8,  true },
    {stmt_execute_null_bitmap_offset, 9,  true },
    {stmt_execute_null_bitmap_offset, 10, true },
    {stmt_execute_null_bitmap_offset, 11, true },
    {stmt_execute_null_bitmap_offset, 12, true },
    {stmt_execute_null_bitmap_offset, 13, true },
    {stmt_execute_null_bitmap_offset, 14, true },
    {stmt_execute_null_bitmap_offset, 15, true },
    {stmt_execute_null_bitmap_offset, 16, false},

    {binary_row_null_bitmap_offset,   0,  true },
    {binary_row_null_bitmap_offset,   1,  false},
    {binary_row_null_bitmap_offset,   2,  true },
    {binary_row_null_bitmap_offset,   3,  true },
    {binary_row_null_bitmap_offset,   4,  false},
    {binary_row_null_bitmap_offset,   5,  true },
    {binary_row_null_bitmap_offset,   6,  true },
    {binary_row_null_bitmap_offset,   7,  true },
    {binary_row_null_bitmap_offset,   8,  true },
    {binary_row_null_bitmap_offset,   9,  true },
    {binary_row_null_bitmap_offset,   10, true },
    {binary_row_null_bitmap_offset,   11, true },
    {binary_row_null_bitmap_offset,   12, true },
    {binary_row_null_bitmap_offset,   13, true },
    {binary_row_null_bitmap_offset,   14, false},
    {binary_row_null_bitmap_offset,   15, false},
    {binary_row_null_bitmap_offset,   16, false},
};

BOOST_DATA_TEST_CASE(is_null, data::make(all_is_null_samples))
{
    // 0b10110100, 0b11111111, 0b00000000
    std::uint8_t content[] = {0xb4, 0xff, 0x00};
    null_bitmap_traits traits(sample.offset, 17);  // 17 fields
    bool actual = traits.is_null(content, sample.pos);
    BOOST_TEST(actual == sample.expected);
}

BOOST_AUTO_TEST_CASE(is_null_one_field_stmt_execute_first_bit_zero)
{
    std::uint8_t value = 0x00;
    null_bitmap_traits traits(stmt_execute_null_bitmap_offset, 1);
    BOOST_TEST(!traits.is_null(&value, 0));
}

BOOST_AUTO_TEST_CASE(is_null_one_field_stmt_execute_first_bit_one)
{
    std::uint8_t value = 0x01;
    null_bitmap_traits traits(stmt_execute_null_bitmap_offset, 1);
    BOOST_TEST(traits.is_null(&value, 0));
}

BOOST_AUTO_TEST_CASE(is_null_one_field_binary_row_third_bit_zero)
{
    std::uint8_t value = 0x00;
    null_bitmap_traits traits(binary_row_null_bitmap_offset, 1);
    BOOST_TEST(!traits.is_null(&value, 0));
}

BOOST_AUTO_TEST_CASE(is_null_one_field_binary_row_third_bit_one)
{
    std::uint8_t value = 0x04;  // 0b00000100
    null_bitmap_traits traits(binary_row_null_bitmap_offset, 1);
    BOOST_TEST(traits.is_null(&value, 0));
}

BOOST_AUTO_TEST_SUITE(generator)

BOOST_AUTO_TEST_CASE(coverage)
{
    // Creates a generator and calls it until exhausted
    auto gen_null_bitmap = [](boost::span<const field_view> params) {
        std::vector<std::uint8_t> res;
        null_bitmap_generator gen(params);
        while (!gen.done())
            res.push_back(gen.next());
        return res;
    };

    struct
    {
        const char* name;
        std::vector<field_view> params;
        std::vector<std::uint8_t> expected;
    } test_cases[] = {
        // All combinations for up to 2 values
        {"empty", {}, {}},
        {"N", make_fv_vector(nullptr), {0x01}},
        {"V", make_fv_vector(1), {0x00}},
        {"NV", make_fv_vector(nullptr, 1), {0x01}},
        {"NN", make_fv_vector(nullptr, nullptr), {0x03}},
        {"VV", make_fv_vector(1, 1), {0x00}},
        {"VN", make_fv_vector(1, nullptr), {0x02}},

        // Last value null - checking we set the right bit
        {"VVN", make_fv_vector(1, 1, nullptr), {0x04}},
        {"VVVN", make_fv_vector(1, 1, 1, nullptr), {0x08}},
        {"VVVVN", make_fv_vector(1, 1, 1, 1, nullptr), {0x10}},
        {"VVVVVN", make_fv_vector(1, 1, 1, 1, 1, nullptr), {0x20}},
        {"VVVVVVN", make_fv_vector(1, 1, 1, 1, 1, 1, nullptr), {0x40}},
        {"VVVVVVVN", make_fv_vector(1, 1, 1, 1, 1, 1, 1, nullptr), {0x80}},
        {"VVVVVVVV_N", make_fv_vector(1, 1, 1, 1, 1, 1, 1, 1, nullptr), {0x00, 0x01}},
        {"VVVVVVVV_VN", make_fv_vector(1, 1, 1, 1, 1, 1, 1, 1, 1, nullptr), {0x00, 0x02}},
        {"VVVVVVVV_VVN", make_fv_vector(1, 1, 1, 1, 1, 1, 1, 1, 1, 1, nullptr), {0x00, 0x04}},
        {"VVVVVVVV_VVVN", make_fv_vector(1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, nullptr), {0x00, 0x08}},
        {"VVVVVVVV_VVVVN", make_fv_vector(1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, nullptr), {0x00, 0x10}},
        {"VVVVVVVV_VVVVVN", make_fv_vector(1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, nullptr), {0x00, 0x20}},
        {"VVVVVVVV_VVVVVVN", make_fv_vector(1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, nullptr), {0x00, 0x40}},
        {"VVVVVVVV_VVVVVVVN",
         make_fv_vector(1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, nullptr),
         {0x00, 0x80}},
        {"VVVVVVVV_VVVVVVVV_N",
         make_fv_vector(1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, nullptr),
         {0x00, 0x00, 0x01}},

        // Some arbitrary combinations
        {"VNVVNVVN", make_fv_vector(1, nullptr, 1, 1, nullptr, 1, 1, nullptr), {0x92}},
        {"NVVVVNVV_VVNVN",
         make_fv_vector(nullptr, 1, 1, 1, 1, nullptr, 1, 1, 1, 1, nullptr, 1, nullptr),
         {0x21, 0x14}},
        {"VVVVVVVV_VVVVVVVV_V",
         make_fv_vector(1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1),
         {0x00, 0x00, 0x00}},
        {"NNNNNNNN_NNNNNNNN_NNN", std::vector<field_view>(19, field_view()), {0xff, 0xff, 0x07}},
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            auto actual = gen_null_bitmap(tc.params);
            BOOST_MYSQL_ASSERT_BUFFER_EQUALS(actual, tc.expected);
        }
    }
}

// Spotcheck: generating step-by-step yields the expected results
BOOST_AUTO_TEST_CASE(step_by_step)
{
    // Setup
    auto params = make_fv_arr(
        // byte 1
        nullptr,
        1,
        1,
        1,
        nullptr,
        nullptr,
        1,
        1,

        // byte 2
        1,
        nullptr,
        nullptr,
        1,
        1,
        1,
        1,
        nullptr,

        // byte 3
        1,
        1,
        1,
        nullptr,
        1,
        nullptr
    );
    null_bitmap_generator gen(params);

    // Initiates as not done
    BOOST_TEST(!gen.done());

    // Generate first byte
    BOOST_TEST(gen.next() == 0x31u);
    BOOST_TEST(!gen.done());

    // Generate second byte
    BOOST_TEST(gen.next() == 0x86u);
    BOOST_TEST(!gen.done());

    // Generate last byte
    BOOST_TEST(gen.next() == 0x28u);
    BOOST_TEST(gen.done());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()  // test_null_bitmap_traits
