//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <gtest/gtest.h>
#include <array>
#include "boost/mysql/detail/protocol/null_bitmap_traits.hpp"
#include "boost/mysql/detail/auxiliar/stringize.hpp"
#include "test_common.hpp"

using boost::mysql::detail::null_bitmap_traits;
using boost::mysql::detail::stmt_execute_null_bitmap_offset;
using boost::mysql::detail::binary_row_null_bitmap_offset;
using boost::mysql::detail::stringize;
using namespace boost::mysql::test;

namespace
{

// byte_count()
struct byte_count_testcase : named_tag
{
    std::size_t offset;
    std::size_t num_fields;
    std::size_t expected_value;

    byte_count_testcase(std::size_t offset, std::size_t num_fields, std::size_t expected_value) :
        offset(offset), num_fields(num_fields), expected_value(expected_value)
    {
    }

    std::string name() const
    {
        return stringize("offset_", offset, "_numfields_", num_fields);
    }
};

struct NullBitmapTraitsByteCountTest : testing::TestWithParam<byte_count_testcase> {};

TEST_P(NullBitmapTraitsByteCountTest, Trivial_ReturnsNumberOfBytes)
{
    null_bitmap_traits traits (GetParam().offset, GetParam().num_fields);
    EXPECT_EQ(traits.byte_count(), GetParam().expected_value);
}

INSTANTIATE_TEST_SUITE_P(StmtExecuteOffset, NullBitmapTraitsByteCountTest, testing::Values(
    byte_count_testcase{stmt_execute_null_bitmap_offset, 0, 0},
    byte_count_testcase{stmt_execute_null_bitmap_offset, 1, 1},
    byte_count_testcase{stmt_execute_null_bitmap_offset, 2, 1},
    byte_count_testcase{stmt_execute_null_bitmap_offset, 3, 1},
    byte_count_testcase{stmt_execute_null_bitmap_offset, 4, 1},
    byte_count_testcase{stmt_execute_null_bitmap_offset, 5, 1},
    byte_count_testcase{stmt_execute_null_bitmap_offset, 6, 1},
    byte_count_testcase{stmt_execute_null_bitmap_offset, 7, 1},
    byte_count_testcase{stmt_execute_null_bitmap_offset, 8, 1},
    byte_count_testcase{stmt_execute_null_bitmap_offset, 9, 2},
    byte_count_testcase{stmt_execute_null_bitmap_offset, 10, 2},
    byte_count_testcase{stmt_execute_null_bitmap_offset, 11, 2},
    byte_count_testcase{stmt_execute_null_bitmap_offset, 12, 2},
    byte_count_testcase{stmt_execute_null_bitmap_offset, 13, 2},
    byte_count_testcase{stmt_execute_null_bitmap_offset, 14, 2},
    byte_count_testcase{stmt_execute_null_bitmap_offset, 15, 2},
    byte_count_testcase{stmt_execute_null_bitmap_offset, 16, 2},
    byte_count_testcase{stmt_execute_null_bitmap_offset, 17, 3}
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(BinaryRowOffset, NullBitmapTraitsByteCountTest, testing::Values(
    byte_count_testcase{binary_row_null_bitmap_offset, 0, 1},
    byte_count_testcase{binary_row_null_bitmap_offset, 1, 1},
    byte_count_testcase{binary_row_null_bitmap_offset, 2, 1},
    byte_count_testcase{binary_row_null_bitmap_offset, 3, 1},
    byte_count_testcase{binary_row_null_bitmap_offset, 4, 1},
    byte_count_testcase{binary_row_null_bitmap_offset, 5, 1},
    byte_count_testcase{binary_row_null_bitmap_offset, 6, 1},
    byte_count_testcase{binary_row_null_bitmap_offset, 7, 2},
    byte_count_testcase{binary_row_null_bitmap_offset, 8, 2},
    byte_count_testcase{binary_row_null_bitmap_offset, 9, 2},
    byte_count_testcase{binary_row_null_bitmap_offset, 10, 2},
    byte_count_testcase{binary_row_null_bitmap_offset, 11, 2},
    byte_count_testcase{binary_row_null_bitmap_offset, 12, 2},
    byte_count_testcase{binary_row_null_bitmap_offset, 13, 2},
    byte_count_testcase{binary_row_null_bitmap_offset, 14, 2},
    byte_count_testcase{binary_row_null_bitmap_offset, 15, 3},
    byte_count_testcase{binary_row_null_bitmap_offset, 16, 3},
    byte_count_testcase{binary_row_null_bitmap_offset, 17, 3}
), test_name_generator);

// is_null
struct is_null_testcase : named_tag
{
    std::size_t offset;
    std::size_t pos;
    bool expected;

    is_null_testcase(std::size_t offset, std::size_t pos, bool expected) :
        offset(offset), pos(pos), expected(expected)
    {
    }

    std::string name() const
    {
        return stringize("offset_", offset, "_pos_", pos);
    }
};
struct NullBitmapTraitsIsNullTest : testing::TestWithParam<is_null_testcase> {};

TEST_P(NullBitmapTraitsIsNullTest, Trivial_ReturnsFlagValue)
{
    std::array<std::uint8_t, 3> content { 0xb4, 0xff, 0x00 }; // 0b10110100, 0b11111111, 0b00000000
    null_bitmap_traits traits (GetParam().offset, 17); // 17 fields
    bool actual = traits.is_null(content.data(), GetParam().pos);
    EXPECT_EQ(actual, GetParam().expected);
}

INSTANTIATE_TEST_SUITE_P(StmtExecuteOffset, NullBitmapTraitsIsNullTest, testing::Values(
    is_null_testcase{stmt_execute_null_bitmap_offset, 0, false},
    is_null_testcase{stmt_execute_null_bitmap_offset, 1, false},
    is_null_testcase{stmt_execute_null_bitmap_offset, 2, true},
    is_null_testcase{stmt_execute_null_bitmap_offset, 3, false},
    is_null_testcase{stmt_execute_null_bitmap_offset, 4, true},
    is_null_testcase{stmt_execute_null_bitmap_offset, 5, true},
    is_null_testcase{stmt_execute_null_bitmap_offset, 6, false},
    is_null_testcase{stmt_execute_null_bitmap_offset, 7, true},
    is_null_testcase{stmt_execute_null_bitmap_offset, 8, true},
    is_null_testcase{stmt_execute_null_bitmap_offset, 9, true},
    is_null_testcase{stmt_execute_null_bitmap_offset, 10, true},
    is_null_testcase{stmt_execute_null_bitmap_offset, 11, true},
    is_null_testcase{stmt_execute_null_bitmap_offset, 12, true},
    is_null_testcase{stmt_execute_null_bitmap_offset, 13, true},
    is_null_testcase{stmt_execute_null_bitmap_offset, 14, true},
    is_null_testcase{stmt_execute_null_bitmap_offset, 15, true},
    is_null_testcase{stmt_execute_null_bitmap_offset, 16, false}
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(BinaryRowOffset, NullBitmapTraitsIsNullTest, testing::Values(
    is_null_testcase{binary_row_null_bitmap_offset, 0, true},
    is_null_testcase{binary_row_null_bitmap_offset, 1, false},
    is_null_testcase{binary_row_null_bitmap_offset, 2, true},
    is_null_testcase{binary_row_null_bitmap_offset, 3, true},
    is_null_testcase{binary_row_null_bitmap_offset, 4, false},
    is_null_testcase{binary_row_null_bitmap_offset, 5, true},
    is_null_testcase{binary_row_null_bitmap_offset, 6, true},
    is_null_testcase{binary_row_null_bitmap_offset, 7, true},
    is_null_testcase{binary_row_null_bitmap_offset, 8, true},
    is_null_testcase{binary_row_null_bitmap_offset, 9, true},
    is_null_testcase{binary_row_null_bitmap_offset, 10, true},
    is_null_testcase{binary_row_null_bitmap_offset, 11, true},
    is_null_testcase{binary_row_null_bitmap_offset, 12, true},
    is_null_testcase{binary_row_null_bitmap_offset, 13, true},
    is_null_testcase{binary_row_null_bitmap_offset, 14, false},
    is_null_testcase{binary_row_null_bitmap_offset, 15, false},
    is_null_testcase{binary_row_null_bitmap_offset, 16, false}
), test_name_generator);

TEST(NullBitmapTraits, IsNull_OneFieldStmtExecuteFirstBitZero_ReturnsFalse)
{
    std::uint8_t value = 0x00;
    null_bitmap_traits traits (stmt_execute_null_bitmap_offset, 1);
    EXPECT_FALSE(traits.is_null(&value, 0));
}

TEST(NullBitmapTraits, IsNull_OneFieldStmtExecuteFirstBitOne_ReturnsTrue)
{
    std::uint8_t value = 0x01;
    null_bitmap_traits traits (stmt_execute_null_bitmap_offset, 1);
    EXPECT_TRUE(traits.is_null(&value, 0));
}

TEST(NullBitmapTraits, IsNull_OneFieldBinaryRowThirdBitZero_ReturnsFalse)
{
    std::uint8_t value = 0x00;
    null_bitmap_traits traits (binary_row_null_bitmap_offset, 1);
    EXPECT_FALSE(traits.is_null(&value, 0));
}

TEST(NullBitmapTraits, IsNull_OneFieldBinaryRowThirdBitOne_ReturnsTrue)
{
    std::uint8_t value = 0x04; // 0b00000100
    null_bitmap_traits traits (binary_row_null_bitmap_offset, 1);
    EXPECT_TRUE(traits.is_null(&value, 0));
}

// set_null
struct set_null_testcase : named_tag
{
    std::size_t offset;
    std::size_t pos;
    std::array<std::uint8_t, 3> expected;

    set_null_testcase(std::size_t offset, std::size_t pos, const std::array<std::uint8_t, 3>& expected):
        offset(offset), pos(pos), expected(expected) {};

    std::string name() const
    {
        return stringize("offset_", offset, "_pos_", pos);
    }
};
struct NullBitmapTraitsSetNullTest : testing::TestWithParam<set_null_testcase> {};

TEST_P(NullBitmapTraitsSetNullTest, Trivial_SetsAdequateBit)
{
    std::array<std::uint8_t, 4> expected_buffer {}; // help detect buffer overruns
    memcpy(expected_buffer.data(), GetParam().expected.data(), 3);
    std::array<std::uint8_t, 4> actual_buffer {};
    null_bitmap_traits traits (GetParam().offset, 17); // 17 fields
    traits.set_null(actual_buffer.data(), GetParam().pos);
    EXPECT_EQ(expected_buffer, actual_buffer);
}

INSTANTIATE_TEST_SUITE_P(StmtExecuteOffset, NullBitmapTraitsSetNullTest, testing::Values(
    set_null_testcase(stmt_execute_null_bitmap_offset, 0, {0x01, 0, 0}),
    set_null_testcase(stmt_execute_null_bitmap_offset, 1, {0x02, 0, 0}),
    set_null_testcase(stmt_execute_null_bitmap_offset, 2, {0x04, 0, 0}),
    set_null_testcase(stmt_execute_null_bitmap_offset, 3, {0x08, 0, 0}),
    set_null_testcase(stmt_execute_null_bitmap_offset, 4, {0x10, 0, 0}),
    set_null_testcase(stmt_execute_null_bitmap_offset, 5, {0x20, 0, 0}),
    set_null_testcase(stmt_execute_null_bitmap_offset, 6, {0x40, 0, 0}),
    set_null_testcase(stmt_execute_null_bitmap_offset, 7, {0x80, 0, 0}),
    set_null_testcase(stmt_execute_null_bitmap_offset, 8, {0,  0x01, 0}),
    set_null_testcase(stmt_execute_null_bitmap_offset, 9, {0,  0x02, 0}),
    set_null_testcase(stmt_execute_null_bitmap_offset, 10, {0, 0x04, 0}),
    set_null_testcase(stmt_execute_null_bitmap_offset, 11, {0, 0x08, 0}),
    set_null_testcase(stmt_execute_null_bitmap_offset, 12, {0, 0x10, 0}),
    set_null_testcase(stmt_execute_null_bitmap_offset, 13, {0, 0x20, 0}),
    set_null_testcase(stmt_execute_null_bitmap_offset, 14, {0, 0x40, 0}),
    set_null_testcase(stmt_execute_null_bitmap_offset, 15, {0, 0x80, 0}),
    set_null_testcase(stmt_execute_null_bitmap_offset, 16, {0, 0, 0x01})
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(BinaryRowOffset, NullBitmapTraitsSetNullTest, testing::Values(
    set_null_testcase(binary_row_null_bitmap_offset, 0, {0x04, 0, 0}),
    set_null_testcase(binary_row_null_bitmap_offset, 1, {0x08, 0, 0}),
    set_null_testcase(binary_row_null_bitmap_offset, 2, {0x10, 0, 0}),
    set_null_testcase(binary_row_null_bitmap_offset, 3, {0x20, 0, 0}),
    set_null_testcase(binary_row_null_bitmap_offset, 4, {0x40, 0, 0}),
    set_null_testcase(binary_row_null_bitmap_offset, 5, {0x80, 0, 0}),
    set_null_testcase(binary_row_null_bitmap_offset, 6,  {0,  0x01, 0}),
    set_null_testcase(binary_row_null_bitmap_offset, 7,  {0,  0x02, 0}),
    set_null_testcase(binary_row_null_bitmap_offset, 8,  {0,  0x04, 0}),
    set_null_testcase(binary_row_null_bitmap_offset, 9,  {0,  0x08, 0}),
    set_null_testcase(binary_row_null_bitmap_offset, 10, {0,  0x10, 0}),
    set_null_testcase(binary_row_null_bitmap_offset, 11, {0,  0x20, 0}),
    set_null_testcase(binary_row_null_bitmap_offset, 12, {0,  0x40, 0}),
    set_null_testcase(binary_row_null_bitmap_offset, 13, {0,  0x80, 0}),
    set_null_testcase(binary_row_null_bitmap_offset, 14, {0, 0, 0x01}),
    set_null_testcase(binary_row_null_bitmap_offset, 15, {0, 0, 0x02}),
    set_null_testcase(binary_row_null_bitmap_offset, 16, {0, 0, 0x04})
), test_name_generator);

TEST(NullBitmapTraits, SetNull_OneFieldStmtExecute_SetsFirstBitToZero)
{
    std::uint8_t value = 0;
    null_bitmap_traits traits (stmt_execute_null_bitmap_offset, 1);
    traits.set_null(&value, 0);
    EXPECT_EQ(value, 1);
}

TEST(NullBitmapTraits, SetNull_OneFieldBinaryRow_SetsThirdBitToZero)
{
    std::uint8_t value = 0;
    null_bitmap_traits traits (binary_row_null_bitmap_offset, 1);
    traits.set_null(&value, 0);
    EXPECT_EQ(value, 4);
}

TEST(NullBitmapTraits, SetNull_MultifiedStmtExecute_SetsAppropriateBits)
{
    std::array<std::uint8_t, 4> expected_buffer { 0xb4, 0xff, 0x00, 0x00 };
    std::array<std::uint8_t, 4> actual_buffer {};
    null_bitmap_traits traits (stmt_execute_null_bitmap_offset, 17); // 17 fields
    for (std::size_t pos: {2, 4, 5, 7, 8, 9, 10, 11, 12, 13, 14, 15})
    {
        traits.set_null(actual_buffer.data(), pos);
    }
    EXPECT_EQ(expected_buffer, actual_buffer);
}

TEST(NullBitmapTraits, SetNull_MultifiedBinaryRow_SetsAppropriateBits)
{
    std::array<std::uint8_t, 4> expected_buffer { 0xb4, 0xff, 0x00, 0x00 };
    std::array<std::uint8_t, 4> actual_buffer {};
    null_bitmap_traits traits (binary_row_null_bitmap_offset, 17); // 17 fields
    for (std::size_t pos: {0, 2, 3, 5, 6, 7, 8, 9, 10, 11, 12, 13})
    {
        traits.set_null(actual_buffer.data(), pos);
    }
    EXPECT_EQ(expected_buffer, actual_buffer);
}


} // anon namespace
