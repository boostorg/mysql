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
), [](auto v) { return "first_byte_" + to_string(v.param.first_byte); });

