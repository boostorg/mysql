/*
 * deserialization.cpp
 *
 *  Created on: Jun 29, 2019
 *      Author: ruben
 */

#include "deserialization.hpp"
#include <gtest/gtest.h>
#include <string>

using namespace std;
using namespace mysql;

// Fixed size integers

template <typename T> constexpr std::size_t int_size = sizeof(T);
template <> constexpr std::size_t int_size<int3> = 3;
template <> constexpr std::size_t int_size<int6> = 6;

template <typename T> constexpr T expected_int_value;
template <> constexpr int1 expected_int_value<int1> { 0xff };
template <> constexpr int2 expected_int_value<int2> { 0xfeff };
template <> constexpr int3 expected_int_value<int3> { 0xfdfeff };
template <> constexpr int4 expected_int_value<int4> { 0xfcfdfeff };
template <> constexpr int6 expected_int_value<int6> { 0xfafbfcfdfeff };
template <> constexpr int8 expected_int_value<int8> { 0xf8f9fafbfcfdfeff };

template <typename T> constexpr auto get_int_underlying_value(T from) { return from; }
constexpr uint32_t get_int_underlying_value(int3 from) { return from.value; }
constexpr uint64_t get_int_underlying_value(int6 from) { return from.value; }

template <typename T>
struct DeserializeFixedSizeInt : public ::testing::Test {
	uint8_t buffer [16];
	DeserializeFixedSizeInt():
		buffer { 0xff, 0xfe, 0xfd, 0xfc, 0xfb, 0xfa, 0xf9, 0xf8, 0xf7 }
	{};
};

using FixedSizeIntTypes = ::testing::Types<int1, int2, int3, int4, int6, int8>;
TYPED_TEST_SUITE(DeserializeFixedSizeInt, FixedSizeIntTypes);

TYPED_TEST(DeserializeFixedSizeInt, ExactSize_GetsValueIncrementsIterator)
{
	TypeParam value;
	auto res = deserialize(this->buffer, this->buffer + int_size<TypeParam>, value);
	EXPECT_EQ(res, this->buffer+int_size<TypeParam>);
	EXPECT_EQ(get_int_underlying_value(value), get_int_underlying_value(expected_int_value<TypeParam>));
}

TYPED_TEST(DeserializeFixedSizeInt, ExtraSize_GetsValueIncrementsIterator)
{
	TypeParam value;
	auto res = deserialize(this->buffer, this->buffer + int_size<TypeParam> + 1, value);
	EXPECT_EQ(res, this->buffer+int_size<TypeParam>);
	EXPECT_EQ(get_int_underlying_value(value), get_int_underlying_value(expected_int_value<TypeParam>));
}

TYPED_TEST(DeserializeFixedSizeInt, Overflow_ThrowsOutOfRange)
{
	TypeParam value;
	EXPECT_THROW(deserialize(this->buffer, this->buffer + int_size<TypeParam> - 1, value), out_of_range);
}

// Length-encoded integer
struct LengthEncodedIntTestParams
{
	uint8_t first_byte;
	uint64_t expected;
	size_t buffer_size;
};
struct DeserializeLengthEncodedInt : public ::testing::TestWithParam<LengthEncodedIntTestParams> {};

TEST_P(DeserializeLengthEncodedInt, ExactSize_GetsValueIncrementsIterator)
{
	uint8_t buffer [10] = { GetParam().first_byte, 0xff, 0xfe, 0xfd, 0xfc, 0xfb, 0xfa, 0xf9, 0xf8 };
	int_lenenc value;
	auto it = deserialize(buffer, buffer + GetParam().buffer_size , value);
	EXPECT_EQ(it, buffer + GetParam().buffer_size);
	EXPECT_EQ(value.value, GetParam().expected);
}

TEST_P(DeserializeLengthEncodedInt, ExtraSize_GetsValueIncrementsIterator)
{
	uint8_t buffer [10] = { GetParam().first_byte, 0xff, 0xfe, 0xfd, 0xfc, 0xfb, 0xfa, 0xf9, 0xf8, 0xf7 };
	int_lenenc value;
	auto it = deserialize(buffer, end(buffer), value);
	EXPECT_EQ(it, buffer + GetParam().buffer_size);
	EXPECT_EQ(value.value, GetParam().expected);
}

TEST_P(DeserializeLengthEncodedInt, Overflow_ThrowsOutOfRange)
{
	uint8_t buffer [10] = { GetParam().first_byte, 0xff, 0xfe, 0xfd, 0xfc, 0xfb, 0xfa, 0xf9, 0xf8, 0xf7 };
	int_lenenc value;
	EXPECT_THROW(deserialize(buffer, buffer + GetParam().buffer_size - 1, value), out_of_range);
}

INSTANTIATE_TEST_SUITE_P(Default, DeserializeLengthEncodedInt, ::testing::Values(
	LengthEncodedIntTestParams{0x0a, 0x0a,               1},
	LengthEncodedIntTestParams{0xfc, 0xfeff,             3},
	LengthEncodedIntTestParams{0xfd, 0xfdfeff,           4},
	LengthEncodedIntTestParams{0xfe, 0xf8f9fafbfcfdfeff, 9}
), [](const auto& v) { return "first_byte_" + to_string(v.param.first_byte); });

// Fixed size string
struct DeserializeFixedSizeString : public testing::Test
{
	uint8_t buffer [6] { 'a', 'b', '\0', 'd', 'e', 'f' };
	string_fixed<5> value;
};

TEST_F(DeserializeFixedSizeString, ExactSize_CopiesValueIncrementsIterator)
{
	ReadIterator res = deserialize(begin(buffer), begin(buffer) + 5, value);
	EXPECT_EQ(value, string_view {"ab\0de"});
	EXPECT_EQ(res, begin(buffer) + 5);
}

TEST_F(DeserializeFixedSizeString, ExtraSize_CopiesValueIncrementsIterator)
{
	ReadIterator res = deserialize(begin(buffer), end(buffer), value);
	EXPECT_EQ(value, string_view {"ab\0de"});
	EXPECT_EQ(res, begin(buffer) + 5);
}

TEST_F(DeserializeFixedSizeString, Overflow_ThrowsOutOfRange)
{
	EXPECT_THROW(deserialize(begin(buffer), begin(buffer) + 4, value), out_of_range);
}

// Null-terminated string
struct DeserializeNullTerminatedString : public testing::Test
{
	uint8_t buffer [4] { 'a', 'b', '\0', 'd' };
	string_null value;
};

TEST_F(DeserializeNullTerminatedString, ExactSize_GetsValueIncrementsIterator)
{
	ReadIterator res = deserialize(begin(buffer), begin(buffer) + 3, value);
	EXPECT_EQ(value.value, "ab");
	EXPECT_EQ(res, begin(buffer) + 3);
}

TEST_F(DeserializeNullTerminatedString, ExtraSize_GetsValueIncrementsIterator)
{
	ReadIterator res = deserialize(begin(buffer), end(buffer), value);
	EXPECT_EQ(value.value, "ab");
	EXPECT_EQ(res, begin(buffer) + 3);
}

TEST_F(DeserializeNullTerminatedString, Overflow_ThrowsOutOfRange)
{
	EXPECT_THROW(deserialize(begin(buffer), begin(buffer) + 2, value), out_of_range);
}

// Length-encoded string
struct LengthEncodedStringParams
{
	uint64_t string_length;
	std::vector<uint8_t> length_prefix;
};

struct DeserializeLengthEncodedString : public ::testing::TestWithParam<LengthEncodedStringParams>
{
	std::vector<uint8_t> buffer;
	string_lenenc value;
	DeserializeLengthEncodedString()
	{
		const auto& prefix = GetParam().length_prefix;
		copy(prefix.begin(), prefix.end(), back_inserter(buffer));
		buffer.resize(buffer.size() + GetParam().string_length + 8, 'a');
	}
	ReadIterator exact_end() const { return buffer.data() + buffer.size() - 8; };
	ReadIterator extra_end() const { return buffer.data() + buffer.size(); }
	ReadIterator overflow_string_end() const { return buffer.data() + buffer.size() - 9; }
	ReadIterator overflow_int_end() const { return buffer.data() + GetParam().length_prefix.size() - 1; }
	string expected_value() const { return string(GetParam().string_length, 'a'); }
};

TEST_P(DeserializeLengthEncodedString, ExactSize_GetsValueIncrementsIterator)
{
	ReadIterator res = deserialize(buffer.data(), exact_end(), value);
	EXPECT_EQ(res, exact_end());
	EXPECT_EQ(value.value, expected_value());
}

TEST_P(DeserializeLengthEncodedString, ExtraSize_GetsValueIncrementsIterator)
{
	ReadIterator res = deserialize(buffer.data(), extra_end(), value);
	EXPECT_EQ(res, exact_end());
	EXPECT_EQ(value.value, expected_value());
}

TEST_P(DeserializeLengthEncodedString, OverflowInString_ThrowsOutOfRange)
{
	EXPECT_THROW(deserialize(buffer.data(), overflow_string_end(), value), out_of_range);
}

TEST_P(DeserializeLengthEncodedString, OverflowInInt_ThrowsOutOfRange)
{
	EXPECT_THROW(deserialize(buffer.data(), overflow_int_end(), value), out_of_range);
	EXPECT_THROW(deserialize(buffer.data(), buffer.data(), value), out_of_range);
}

INSTANTIATE_TEST_SUITE_P(Default, DeserializeLengthEncodedString, ::testing::Values(
	LengthEncodedStringParams{0x10,        {0x10}},
	LengthEncodedStringParams{0xfeff,      {0xfc, 0xff, 0xfe}},
	LengthEncodedStringParams{0xfdfeff,    {0xfd, 0xff, 0xfe, 0xfd}}
	// Allocating strings as long as 0x100000000 can cause bad_alloc
), [](const auto& v) { return "string_length_" + to_string(v.param.string_length); });

// EOF string
struct DeserializeEofString : public testing::Test
{
	uint8_t buffer [4] { 'a', 'b', '\0', 'd' };
	string_eof value;
};

TEST_F(DeserializeEofString, Trivial_GetsValueIncrementsIterator)
{
	string_view expected {"ab\0d", 4};
	ReadIterator res = deserialize(begin(buffer), end(buffer), value);
	EXPECT_EQ(value.value, expected);
	EXPECT_EQ(res, end(buffer));
}

TEST_F(DeserializeEofString, EmptyBuffer_GetsEmptyValue)
{
	ReadIterator res = deserialize(begin(buffer), begin(buffer), value);
	EXPECT_EQ(value.value, "");
	EXPECT_EQ(res, begin(buffer));
}

// Enums
enum class TestEnum : int2
{
	value0 = 0,
	value1 = 0xfeff
};

struct DeserializeEnum : public testing::Test
{
	uint8_t buffer [3] { 0xff, 0xfe, 0xaa };
	TestEnum value;
};

TEST_F(DeserializeEnum, ExactSize_GetsValueIncrementsIterator)
{
	ReadIterator res = deserialize(begin(buffer), begin(buffer) + 2, value);
	EXPECT_EQ(res, begin(buffer) + 2);
	EXPECT_EQ(value, TestEnum::value1);
}

TEST_F(DeserializeEnum, ExtraSize_GetsValueIncrementsIterator)
{
	ReadIterator res = deserialize(begin(buffer), end(buffer), value);
	EXPECT_EQ(res, begin(buffer) + 2);
	EXPECT_EQ(value, TestEnum::value1);
}

TEST_F(DeserializeEnum, Overflow_ThrowsOutOfRange)
{
	EXPECT_THROW(deserialize(begin(buffer), begin(buffer) + 1, value), out_of_range);
}
