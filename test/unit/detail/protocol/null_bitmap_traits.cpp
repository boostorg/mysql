//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <gtest/gtest.h>
#include <array>
#include "boost/mysql/detail/protocol/null_bitmap_traits.hpp"
#include "test_common.hpp"

using boost::mysql::detail::null_bitmap_traits;
using boost::mysql::detail::stmt_execute_null_bitmap_offset;
using boost::mysql::detail::binary_row_null_bitmap_offset;
using namespace boost::mysql::test;

namespace
{

// byte_count()
struct ByteCountParams
{
    std::size_t offset;
    std::size_t num_fields;
    std::size_t expected_value;
};
std::ostream& operator<<(std::ostream& os, const ByteCountParams& v)
{
    return os << "offset=" << v.offset << ",num_fields=" << v.num_fields;
}

struct NullBitmapTraitsByteCountTest : testing::TestWithParam<ByteCountParams> {};

TEST_P(NullBitmapTraitsByteCountTest, Trivial_ReturnsNumberOfBytes)
{
    null_bitmap_traits traits (GetParam().offset, GetParam().num_fields);
    EXPECT_EQ(traits.byte_count(), GetParam().expected_value);
}

INSTANTIATE_TEST_SUITE_P(StmtExecuteOffset, NullBitmapTraitsByteCountTest, testing::Values(
    ByteCountParams{stmt_execute_null_bitmap_offset, 0, 0},
    ByteCountParams{stmt_execute_null_bitmap_offset, 1, 1},
    ByteCountParams{stmt_execute_null_bitmap_offset, 2, 1},
    ByteCountParams{stmt_execute_null_bitmap_offset, 3, 1},
    ByteCountParams{stmt_execute_null_bitmap_offset, 4, 1},
    ByteCountParams{stmt_execute_null_bitmap_offset, 5, 1},
    ByteCountParams{stmt_execute_null_bitmap_offset, 6, 1},
    ByteCountParams{stmt_execute_null_bitmap_offset, 7, 1},
    ByteCountParams{stmt_execute_null_bitmap_offset, 8, 1},
    ByteCountParams{stmt_execute_null_bitmap_offset, 9, 2},
    ByteCountParams{stmt_execute_null_bitmap_offset, 10, 2},
    ByteCountParams{stmt_execute_null_bitmap_offset, 11, 2},
    ByteCountParams{stmt_execute_null_bitmap_offset, 12, 2},
    ByteCountParams{stmt_execute_null_bitmap_offset, 13, 2},
    ByteCountParams{stmt_execute_null_bitmap_offset, 14, 2},
    ByteCountParams{stmt_execute_null_bitmap_offset, 15, 2},
    ByteCountParams{stmt_execute_null_bitmap_offset, 16, 2},
    ByteCountParams{stmt_execute_null_bitmap_offset, 17, 3}
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(BinaryRowOffset, NullBitmapTraitsByteCountTest, testing::Values(
    ByteCountParams{binary_row_null_bitmap_offset, 0, 1},
    ByteCountParams{binary_row_null_bitmap_offset, 1, 1},
    ByteCountParams{binary_row_null_bitmap_offset, 2, 1},
    ByteCountParams{binary_row_null_bitmap_offset, 3, 1},
    ByteCountParams{binary_row_null_bitmap_offset, 4, 1},
    ByteCountParams{binary_row_null_bitmap_offset, 5, 1},
    ByteCountParams{binary_row_null_bitmap_offset, 6, 1},
    ByteCountParams{binary_row_null_bitmap_offset, 7, 2},
    ByteCountParams{binary_row_null_bitmap_offset, 8, 2},
    ByteCountParams{binary_row_null_bitmap_offset, 9, 2},
    ByteCountParams{binary_row_null_bitmap_offset, 10, 2},
    ByteCountParams{binary_row_null_bitmap_offset, 11, 2},
    ByteCountParams{binary_row_null_bitmap_offset, 12, 2},
    ByteCountParams{binary_row_null_bitmap_offset, 13, 2},
    ByteCountParams{binary_row_null_bitmap_offset, 14, 2},
    ByteCountParams{binary_row_null_bitmap_offset, 15, 3},
    ByteCountParams{binary_row_null_bitmap_offset, 16, 3},
    ByteCountParams{binary_row_null_bitmap_offset, 17, 3}
), test_name_generator);

// is_null
struct IsNullParams
{
    std::size_t offset;
    std::size_t pos;
    bool expected;
};
std::ostream& operator<<(std::ostream& os, const IsNullParams& v)
{
    return os << "offset=" << v.offset << ",pos=" << v.pos;
}
struct NullBitmapTraitsIsNullTest : testing::TestWithParam<IsNullParams> {};

TEST_P(NullBitmapTraitsIsNullTest, Trivial_ReturnsFlagValue)
{
    std::array<std::uint8_t, 3> content { 0xb4, 0xff, 0x00 }; // 0b10110100, 0b11111111, 0b00000000
    null_bitmap_traits traits (GetParam().offset, 17); // 17 fields
    bool actual = traits.is_null(content.data(), GetParam().pos);
    EXPECT_EQ(actual, GetParam().expected);
}

INSTANTIATE_TEST_SUITE_P(StmtExecuteOffset, NullBitmapTraitsIsNullTest, testing::Values(
    IsNullParams{stmt_execute_null_bitmap_offset, 0, false},
    IsNullParams{stmt_execute_null_bitmap_offset, 1, false},
    IsNullParams{stmt_execute_null_bitmap_offset, 2, true},
    IsNullParams{stmt_execute_null_bitmap_offset, 3, false},
    IsNullParams{stmt_execute_null_bitmap_offset, 4, true},
    IsNullParams{stmt_execute_null_bitmap_offset, 5, true},
    IsNullParams{stmt_execute_null_bitmap_offset, 6, false},
    IsNullParams{stmt_execute_null_bitmap_offset, 7, true},
    IsNullParams{stmt_execute_null_bitmap_offset, 8, true},
    IsNullParams{stmt_execute_null_bitmap_offset, 9, true},
    IsNullParams{stmt_execute_null_bitmap_offset, 10, true},
    IsNullParams{stmt_execute_null_bitmap_offset, 11, true},
    IsNullParams{stmt_execute_null_bitmap_offset, 12, true},
    IsNullParams{stmt_execute_null_bitmap_offset, 13, true},
    IsNullParams{stmt_execute_null_bitmap_offset, 14, true},
    IsNullParams{stmt_execute_null_bitmap_offset, 15, true},
    IsNullParams{stmt_execute_null_bitmap_offset, 16, false}
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(BinaryRowOffset, NullBitmapTraitsIsNullTest, testing::Values(
    IsNullParams{binary_row_null_bitmap_offset, 0, true},
    IsNullParams{binary_row_null_bitmap_offset, 1, false},
    IsNullParams{binary_row_null_bitmap_offset, 2, true},
    IsNullParams{binary_row_null_bitmap_offset, 3, true},
    IsNullParams{binary_row_null_bitmap_offset, 4, false},
    IsNullParams{binary_row_null_bitmap_offset, 5, true},
    IsNullParams{binary_row_null_bitmap_offset, 6, true},
    IsNullParams{binary_row_null_bitmap_offset, 7, true},
    IsNullParams{binary_row_null_bitmap_offset, 8, true},
    IsNullParams{binary_row_null_bitmap_offset, 9, true},
    IsNullParams{binary_row_null_bitmap_offset, 10, true},
    IsNullParams{binary_row_null_bitmap_offset, 11, true},
    IsNullParams{binary_row_null_bitmap_offset, 12, true},
    IsNullParams{binary_row_null_bitmap_offset, 13, true},
    IsNullParams{binary_row_null_bitmap_offset, 14, false},
    IsNullParams{binary_row_null_bitmap_offset, 15, false},
    IsNullParams{binary_row_null_bitmap_offset, 16, false}
), test_name_generator);

TEST(NullBitmapTraits, IsNull_OneFieldStmtExecuteFirstBitZero_ReturnsFalse)
{
    std::uint8_t value = 0b00000000;
    null_bitmap_traits traits (stmt_execute_null_bitmap_offset, 1);
    EXPECT_FALSE(traits.is_null(&value, 0));
}

TEST(NullBitmapTraits, IsNull_OneFieldStmtExecuteFirstBitOne_ReturnsTrue)
{
    std::uint8_t value = 0b00000001;
    null_bitmap_traits traits (stmt_execute_null_bitmap_offset, 1);
    EXPECT_TRUE(traits.is_null(&value, 0));
}

TEST(NullBitmapTraits, IsNull_OneFieldBinaryRowThirdBitZero_ReturnsFalse)
{
    std::uint8_t value = 0b00000000;
    null_bitmap_traits traits (binary_row_null_bitmap_offset, 1);
    EXPECT_FALSE(traits.is_null(&value, 0));
}

TEST(NullBitmapTraits, IsNull_OneFieldBinaryRowThirdBitOne_ReturnsTrue)
{
    std::uint8_t value = 0b00000100;
    null_bitmap_traits traits (binary_row_null_bitmap_offset, 1);
    EXPECT_TRUE(traits.is_null(&value, 0));
}

// set_null
struct SetNullParams
{
    std::size_t offset;
    std::size_t pos;
    std::array<std::uint8_t, 3> expected;

    SetNullParams(std::size_t offset, std::size_t pos, const std::array<std::uint8_t, 3>& expected):
        offset(offset), pos(pos), expected(expected) {};
};
std::ostream& operator<<(std::ostream& os, const SetNullParams& v)
{
    return os << "offset=" << v.offset << ",pos=" << v.pos;
}
struct NullBitmapTraitsSetNullTest : testing::TestWithParam<SetNullParams> {};

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
    SetNullParams(stmt_execute_null_bitmap_offset, 0, {0b00000001, 0, 0}),
    SetNullParams(stmt_execute_null_bitmap_offset, 1, {0b00000010, 0, 0}),
    SetNullParams(stmt_execute_null_bitmap_offset, 2, {0b00000100, 0, 0}),
    SetNullParams(stmt_execute_null_bitmap_offset, 3, {0b00001000, 0, 0}),
    SetNullParams(stmt_execute_null_bitmap_offset, 4, {0b00010000, 0, 0}),
    SetNullParams(stmt_execute_null_bitmap_offset, 5, {0b00100000, 0, 0}),
    SetNullParams(stmt_execute_null_bitmap_offset, 6, {0b01000000, 0, 0}),
    SetNullParams(stmt_execute_null_bitmap_offset, 7, {0b10000000, 0, 0}),
    SetNullParams(stmt_execute_null_bitmap_offset, 8, {0,  0b00000001, 0}),
    SetNullParams(stmt_execute_null_bitmap_offset, 9, {0,  0b00000010, 0}),
    SetNullParams(stmt_execute_null_bitmap_offset, 10, {0, 0b00000100, 0}),
    SetNullParams(stmt_execute_null_bitmap_offset, 11, {0, 0b00001000, 0}),
    SetNullParams(stmt_execute_null_bitmap_offset, 12, {0, 0b00010000, 0}),
    SetNullParams(stmt_execute_null_bitmap_offset, 13, {0, 0b00100000, 0}),
    SetNullParams(stmt_execute_null_bitmap_offset, 14, {0, 0b01000000, 0}),
    SetNullParams(stmt_execute_null_bitmap_offset, 15, {0, 0b10000000, 0}),
    SetNullParams(stmt_execute_null_bitmap_offset, 16, {0, 0, 0b00000001})
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(BinaryRowOffset, NullBitmapTraitsSetNullTest, testing::Values(
    SetNullParams(binary_row_null_bitmap_offset, 0, {0b00000100, 0, 0}),
    SetNullParams(binary_row_null_bitmap_offset, 1, {0b00001000, 0, 0}),
    SetNullParams(binary_row_null_bitmap_offset, 2, {0b00010000, 0, 0}),
    SetNullParams(binary_row_null_bitmap_offset, 3, {0b00100000, 0, 0}),
    SetNullParams(binary_row_null_bitmap_offset, 4, {0b01000000, 0, 0}),
    SetNullParams(binary_row_null_bitmap_offset, 5, {0b10000000, 0, 0}),
    SetNullParams(binary_row_null_bitmap_offset, 6,  {0,  0b00000001, 0}),
    SetNullParams(binary_row_null_bitmap_offset, 7,  {0,  0b00000010, 0}),
    SetNullParams(binary_row_null_bitmap_offset, 8,  {0, 0b00000100, 0}),
    SetNullParams(binary_row_null_bitmap_offset, 9,  {0, 0b00001000, 0}),
    SetNullParams(binary_row_null_bitmap_offset, 10, {0, 0b00010000, 0}),
    SetNullParams(binary_row_null_bitmap_offset, 11, {0, 0b00100000, 0}),
    SetNullParams(binary_row_null_bitmap_offset, 12, {0, 0b01000000, 0}),
    SetNullParams(binary_row_null_bitmap_offset, 13, {0, 0b10000000, 0}),
    SetNullParams(binary_row_null_bitmap_offset, 14, {0, 0, 0b00000001}),
    SetNullParams(binary_row_null_bitmap_offset, 15, {0, 0, 0b00000010}),
    SetNullParams(binary_row_null_bitmap_offset, 16, {0, 0, 0b00000100})
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



}

