/*
 * deserialization.cpp
 *
 *  Created on: Jun 29, 2019
 *      Author: ruben
 */

#include "mysql/impl/basic_serialization.hpp"
#include <gtest/gtest.h>
#include <string>

using namespace testing;
using namespace std;
using namespace mysql;
using namespace mysql::detail;

namespace
{

// Fixed size integers
template <typename T> constexpr std::size_t int_size = sizeof(T::value);
template <> constexpr std::size_t int_size<int3> = 3;
template <> constexpr std::size_t int_size<int6> = 6;

template <typename T> constexpr T expected_int_value();
template <> constexpr int1 expected_int_value<int1>() { return int1{0xff}; };
template <> constexpr int2 expected_int_value<int2>() { return int2{0xfeff}; };
template <> constexpr int3 expected_int_value<int3>() { return int3{0xfdfeff}; };
template <> constexpr int4 expected_int_value<int4>() { return int4{0xfcfdfeff}; };
template <> constexpr int6 expected_int_value<int6>() { return int6{0xfafbfcfdfeff}; };
template <> constexpr int8 expected_int_value<int8>() { return int8{0xf8f9fafbfcfdfeff}; };
template <> constexpr int1_signed expected_int_value<int1_signed>() { return int1_signed{-1}; };
template <> constexpr int2_signed expected_int_value<int2_signed>() { return int2_signed{-0x101}; };
template <> constexpr int4_signed expected_int_value<int4_signed>() { return int4_signed{-0x3020101}; };
template <> constexpr int8_signed expected_int_value<int8_signed>() { return int8_signed{-0x0706050403020101}; };

uint8_t fixed_size_int_buffer [16] { 0xff, 0xfe, 0xfd, 0xfc, 0xfb, 0xfa, 0xf9, 0xf8, 0xf7 };

template <typename T>
struct DeserializeFixedSizeInt : public ::testing::Test {
	uint8_t buffer [16];
	T value;

	DeserializeFixedSizeInt()
	{
		memcpy(buffer, fixed_size_int_buffer, sizeof(buffer));
		memset(&value, 1, sizeof(value)); // catch unititialized memory errors
	};
};

using FixedSizeIntTypes = ::testing::Types<
	int1,
	int2,
	int3,
	int4,
	int6,
	int8,
	int1_signed,
	int2_signed,
	int4_signed,
	int8_signed
>;
TYPED_TEST_SUITE(DeserializeFixedSizeInt, FixedSizeIntTypes);

TYPED_TEST(DeserializeFixedSizeInt, ExactSize_GetsValueIncrementsIterator)
{
	DeserializationContext ctx (this->buffer, this->buffer + int_size<TypeParam>, 0);

	auto err = deserialize(this->value, ctx);

	EXPECT_EQ(ctx.first(), this->buffer+int_size<TypeParam>);
	EXPECT_EQ(this->value.value, expected_int_value<TypeParam>().value);
	EXPECT_EQ(err, Error::ok);
}

TYPED_TEST(DeserializeFixedSizeInt, ExtraSize_GetsValueIncrementsIterator)
{
	DeserializationContext ctx (this->buffer, this->buffer + 1 + int_size<TypeParam>, 0);

	auto err = deserialize(this->value, ctx);

	EXPECT_EQ(ctx.first(), this->buffer+int_size<TypeParam>);
	EXPECT_EQ(this->value.value, expected_int_value<TypeParam>().value);
	EXPECT_EQ(err, Error::ok);
}

TYPED_TEST(DeserializeFixedSizeInt, Overflow_ReturnsError)
{
	DeserializationContext ctx (this->buffer, this->buffer - 1 + int_size<TypeParam>, 0);
	auto err = deserialize(this->value, ctx);
	EXPECT_EQ(err, Error::incomplete_message);
}

template <typename T>
struct GetSizeFixedSizeInt : public testing::Test
{
	T value = expected_int_value<T>();
	SerializationContext ctx {0};
};

TYPED_TEST_SUITE(GetSizeFixedSizeInt, FixedSizeIntTypes);

TYPED_TEST(GetSizeFixedSizeInt, Trivial_ReturnsSizeOf)
{
	EXPECT_EQ(get_size(this->value, this->ctx), int_size<TypeParam>);
}

template <typename T>
struct SerializeFixedSizeInt : public testing::Test
{
	uint8_t buffer [16];
	T value = expected_int_value<T>();
	SerializationContext ctx {0, begin(buffer)};

	SerializeFixedSizeInt()
	{
		memset(buffer, 1, sizeof(buffer)); // catch buffer overflow errors
	}
};

TYPED_TEST_SUITE(SerializeFixedSizeInt, FixedSizeIntTypes);

string_view buffer_to_view(const uint8_t* buffer, size_t size)
{
	return string_view(reinterpret_cast<const char*>(buffer), size);
}

TYPED_TEST(SerializeFixedSizeInt, Trivial_WritesBytesToBufferAdvancesIteratorNoOverflow)
{
	auto intsz = int_size<TypeParam>;
	serialize(this->value, this->ctx);

	EXPECT_EQ(this->ctx.first(), begin(this->buffer) + intsz);

	string_view written_buffer = buffer_to_view(this->buffer, intsz);
	string_view expected_written_buffer = buffer_to_view(fixed_size_int_buffer, intsz);
	EXPECT_EQ(written_buffer, expected_written_buffer);

	auto clean_buffer_size = sizeof(this->buffer) - intsz;
	string_view clean_buffer = buffer_to_view(this->buffer + intsz, clean_buffer_size);
	string expected_clean_buffer (clean_buffer_size, '\1');
	EXPECT_EQ(clean_buffer, expected_clean_buffer);
}


// Length-encoded integer
struct DeserializeLengthEncodedIntParams
{
	uint8_t first_byte;
	uint64_t expected;
	size_t buffer_size;
};
struct DeserializeLengthEncodedInt : public ::testing::TestWithParam<DeserializeLengthEncodedIntParams>
{
	uint8_t buffer [10];
	int_lenenc value;

	DeserializeLengthEncodedInt():
		buffer { GetParam().first_byte, 0xff, 0xfe, 0xfd, 0xfc, 0xfb, 0xfa, 0xf9, 0xf8 }
	{
		memset(&value, 1, sizeof(value));
	}
};

TEST_P(DeserializeLengthEncodedInt, ExactSize_GetsValueIncrementsIterator)
{
	DeserializationContext ctx (buffer, buffer + GetParam().buffer_size, 0);
	auto err = deserialize(value, ctx);
	EXPECT_EQ(ctx.first(), buffer + GetParam().buffer_size);
	EXPECT_EQ(value.value, GetParam().expected);
	EXPECT_EQ(err, Error::ok);
}

TEST_P(DeserializeLengthEncodedInt, ExtraSize_GetsValueIncrementsIterator)
{
	DeserializationContext ctx (buffer, end(buffer), 0);
	auto err = deserialize(value, ctx);
	EXPECT_EQ(ctx.first(), buffer + GetParam().buffer_size);
	EXPECT_EQ(value.value, GetParam().expected);
	EXPECT_EQ(err, Error::ok);
}

TEST_P(DeserializeLengthEncodedInt, Overflow_ReturnsError)
{
	DeserializationContext ctx (buffer, buffer + GetParam().buffer_size - 1, 0);
	auto err = deserialize(value, ctx);
	EXPECT_EQ(err, Error::incomplete_message);
}

INSTANTIATE_TEST_SUITE_P(Default, DeserializeLengthEncodedInt, ::testing::Values(
		DeserializeLengthEncodedIntParams{0x0a, 0x0a,               1},
		DeserializeLengthEncodedIntParams{0xfc, 0xfeff,             3},
		DeserializeLengthEncodedIntParams{0xfd, 0xfdfeff,           4},
		DeserializeLengthEncodedIntParams{0xfe, 0xf8f9fafbfcfdfeff, 9}
), [](const auto& v) { return "first_byte_" + to_string(v.param.first_byte); });

// Fixed size string
struct DeserializeFixedSizeString : public testing::Test
{
	uint8_t buffer [6] { 'a', 'b', '\0', 'd', 'e', 'f' };
	string_fixed<5> value;

	DeserializeFixedSizeString()
	{
		memset(value.value.data(), 1, value.value.size());
	}

	string_view value_as_view() const { return string_view(value.value.data(), value.value.size()); }
};

TEST_F(DeserializeFixedSizeString, ExactSize_CopiesValueIncrementsIterator)
{
	DeserializationContext ctx (begin(buffer), begin(buffer) + 5, 0);
	auto err = deserialize(value, ctx);
	EXPECT_EQ(ctx.first(), begin(buffer) + 5);
	EXPECT_EQ(value_as_view(), string_view("ab\0de", 5));
	EXPECT_EQ(err, Error::ok);
}

TEST_F(DeserializeFixedSizeString, ExtraSize_CopiesValueIncrementsIterator)
{
	DeserializationContext ctx (begin(buffer), end(buffer), 0);
	auto err = deserialize(value, ctx);
	EXPECT_EQ(ctx.first(), begin(buffer) + 5);
	EXPECT_EQ(value_as_view(), string_view("ab\0de", 5));
	EXPECT_EQ(err, Error::ok);
}

TEST_F(DeserializeFixedSizeString, Overflow_ReturnsError)
{
	DeserializationContext ctx (begin(buffer), begin(buffer) + 4, 0);
	auto err = deserialize(value, ctx);
	EXPECT_EQ(err, Error::incomplete_message);
}

// Null-terminated string
/*struct DeserializeNullTerminatedString : public testing::Test
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
}*/


} // anon namespace
