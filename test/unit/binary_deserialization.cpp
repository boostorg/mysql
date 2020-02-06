/*
 * deserialize_row.cpp
 *
 *  Created on: Nov 8, 2019
 *      Author: ruben
 */

#include <gtest/gtest.h>
#include "mysql/impl/binary_deserialization.hpp"
#include "test_common.hpp"

using namespace mysql::detail;
using namespace mysql::test;
using namespace testing;
using namespace date::literals;
using mysql::value;
using mysql::collation;
using mysql::error_code;
using mysql::Error;

namespace
{

using mysql::operator<<;

std::vector<mysql::field_metadata> make_meta(
	const std::vector<protocol_field_type>& types
)
{
	std::vector<mysql::field_metadata> res;
	for (const auto type: types)
	{
		column_definition_packet coldef;
		coldef.type = type;
		res.emplace_back(coldef);
	}
	return res;
}

// for deserialize_binary_value
struct BinaryValueParam : named_param
{
	std::string name;
	std::vector<std::uint8_t> from;
	value expected;
	protocol_field_type type;
	std::uint16_t flags;

	template <typename T>
	BinaryValueParam(
		std::string name,
		std::vector<std::uint8_t> from,
		T&& expected_value,
		protocol_field_type type,
		std::uint16_t flags=0
	):
		name(std::move(name)),
		from(std::move(from)),
		expected(std::forward<T>(expected_value)),
		type(type),
		flags(flags)
	{
	}
};

struct DeserializeBinaryValueTest : public TestWithParam<BinaryValueParam> {};

TEST_P(DeserializeBinaryValueTest, CorrectFormat_SetsOutputValueReturnsTrue)
{
	column_definition_packet coldef;
	coldef.type = GetParam().type;
	coldef.flags.value = GetParam().flags;
	mysql::field_metadata meta (coldef);
	value actual_value;
	const auto& buffer = GetParam().from;
	DeserializationContext ctx (buffer.data(), buffer.data() + buffer.size(), capabilities());
	auto err = deserialize_binary_value(ctx, meta, actual_value);
	EXPECT_EQ(err, Error::ok);
	EXPECT_EQ(actual_value, GetParam().expected);
}

INSTANTIATE_TEST_SUITE_P(StringTypes, DeserializeBinaryValueTest, Values(
	BinaryValueParam("varchar", {0x04, 0x74, 0x65, 0x73, 0x74}, "test", protocol_field_type::var_string),
	BinaryValueParam("char", {0x04, 0x74, 0x65, 0x73, 0x74}, "test", protocol_field_type::string),
	BinaryValueParam("varbinary", {0x04, 0x74, 0x65, 0x73, 0x74}, "test",
			protocol_field_type::var_string, column_flags::binary),
	BinaryValueParam("binary", {0x04, 0x74, 0x65, 0x73, 0x74}, "test",
			protocol_field_type::string, column_flags::binary),
	BinaryValueParam("text_blob", {0x04, 0x74, 0x65, 0x73, 0x74}, "test",
			protocol_field_type::blob, column_flags::blob),
	BinaryValueParam("enum", {0x04, 0x74, 0x65, 0x73, 0x74}, "test",
			protocol_field_type::string, column_flags::enum_),
	BinaryValueParam("set", {0x04, 0x74, 0x65, 0x73, 0x74}, "test",
			protocol_field_type::string, column_flags::set),

	BinaryValueParam("bit", {0x02, 0x02, 0x01}, "\2\1", protocol_field_type::bit),
	BinaryValueParam("decimal", {0x02, 0x31, 0x30}, "10", protocol_field_type::newdecimal),
	BinaryValueParam("geomtry", {0x04, 0x74, 0x65, 0x73, 0x74}, "test", protocol_field_type::geometry)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(IntTypes, DeserializeBinaryValueTest, Values(
	BinaryValueParam("tinyint_unsigned", {0x14}, std::uint32_t(20),
			protocol_field_type::tiny, column_flags::unsigned_),
	BinaryValueParam("tinyint_signed", {0xec}, std::int32_t(-20), protocol_field_type::tiny),

	BinaryValueParam("smallint_unsigned", {0x14, 0x00}, std::uint32_t(20),
			protocol_field_type::short_, column_flags::unsigned_),
	BinaryValueParam("smallint_signed", {0xec, 0xff}, std::int32_t(-20), protocol_field_type::short_),

	BinaryValueParam("mediumint_unsigned", {0x14, 0x00, 0x00, 0x00}, std::uint32_t(20),
			protocol_field_type::int24, column_flags::unsigned_),
	BinaryValueParam("mediumint_signed", {0xec, 0xff, 0xff, 0xff}, std::int32_t(-20), protocol_field_type::int24),

	BinaryValueParam("int_unsigned", {0x14, 0x00, 0x00, 0x00}, std::uint32_t(20),
			protocol_field_type::long_, column_flags::unsigned_),
	BinaryValueParam("int_signed", {0xec, 0xff, 0xff, 0xff}, std::int32_t(-20), protocol_field_type::long_),

	BinaryValueParam("bigint_unsigned", {0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, std::uint64_t(20),
			protocol_field_type::longlong, column_flags::unsigned_),
	BinaryValueParam("bigint_signed", {0xec, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, std::int64_t(-20),
			protocol_field_type::longlong)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(FloatingPointTypes, DeserializeBinaryValueTest, Values(
	BinaryValueParam("float", {0x66, 0x66, 0x86, 0xc0}, -4.2f, protocol_field_type::float_),
	BinaryValueParam("double", {0xcd, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x10, 0xc0}, -4.2, protocol_field_type::double_)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(TimeTypes, DeserializeBinaryValueTest, Values(
	BinaryValueParam("date", {0x04, 0xda, 0x07, 0x03, 0x1c}, makedate(2010, 3, 28), protocol_field_type::date),
	BinaryValueParam("datetime", {0x0b, 0xda, 0x07, 0x05, 0x02, 0x17, 0x01, 0x32, 0xa0, 0x86, 0x01, 0x00},
			makedt(2010, 5, 2, 23, 1, 50, 100000), protocol_field_type::datetime),
	BinaryValueParam("timestamp", {0x0b, 0xda, 0x07, 0x05, 0x02, 0x17, 0x01, 0x32, 0xa0, 0x86, 0x01, 0x00},
			makedt(2010, 5, 2, 23, 1, 50, 100000), protocol_field_type::timestamp),
	BinaryValueParam("time", {  0x0c, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x02, 0x03, 0xa0, 0x86, 0x01, 0x00},
			maket(120, 2, 3, 100000), protocol_field_type::time),
	BinaryValueParam("year", {0xe3, 0x07}, std::uint32_t(2019), protocol_field_type::year, column_flags::unsigned_)
), test_name_generator);

// for deserialize_binary_row
struct BinaryRowParam : named_param
{
	std::string name;
	std::vector<std::uint8_t> from;
	std::vector<value> expected;
	std::vector<protocol_field_type> types;

	BinaryRowParam(
		std::string name,
		std::vector<std::uint8_t> from,
		std::vector<value> expected,
		std::vector<protocol_field_type> types
	):
		name(std::move(name)),
		from(std::move(from)),
		expected(std::move(expected)),
		types(std::move(types))
	{
		assert(expected.size() == types.size());
	}
};

struct DeserializeBinaryRowTest : public TestWithParam<BinaryRowParam> {};

TEST_P(DeserializeBinaryRowTest, CorrectFormat_SetsOutputValueReturnsTrue)
{
	auto meta = make_meta(GetParam().types);
	const auto& buffer = GetParam().from;
	DeserializationContext ctx (buffer.data(), buffer.data() + buffer.size(), capabilities());

	std::vector<value> actual;
	auto err = deserialize_binary_row(ctx, meta, actual);
	EXPECT_EQ(err, error_code());
	EXPECT_EQ(actual, GetParam().expected);
}

INSTANTIATE_TEST_SUITE_P(Default, DeserializeBinaryRowTest, testing::Values(
	BinaryRowParam("one_value", {0x00, 0x00, 0x14}, makevalues(std::int32_t(20)), {protocol_field_type::tiny}),
	BinaryRowParam("one_null", {0x00, 0x04}, makevalues(nullptr), {protocol_field_type::tiny}),
	BinaryRowParam("two_values", {0x00, 0x00, 0x03, 0x6d, 0x69, 0x6e, 0x6d, 0x07},
			makevalues("min", std::int32_t(1901)), {protocol_field_type::var_string, protocol_field_type::short_}),
	BinaryRowParam("one_value_one_null", {0x00, 0x08, 0x03, 0x6d, 0x61, 0x78},
			makevalues("max", nullptr), {protocol_field_type::var_string, protocol_field_type::tiny}),
	BinaryRowParam("two_nulls", {0x00, 0x0c},
			makevalues(nullptr, nullptr), {protocol_field_type::tiny, protocol_field_type::tiny}),
	BinaryRowParam("six_nulls", {0x00, 0xfc}, std::vector<value>(6, value(nullptr)),
			std::vector<protocol_field_type>(6, protocol_field_type::tiny)),
	BinaryRowParam("seven_nulls", {0x00, 0xfc, 0x01}, std::vector<value>(7, value(nullptr)),
			std::vector<protocol_field_type>(7, protocol_field_type::tiny)),
	BinaryRowParam("several_values", {
			0x00, 0x90, 0x00, 0xfd, 0x14, 0x00, 0xc3, 0xf5, 0x48,
			0x40, 0x02, 0x61, 0x62, 0x04, 0xe2, 0x07, 0x0a,
			0x05, 0x71, 0x99, 0x6d, 0xe2, 0x93, 0x4d, 0xf5,
			0x3d
		}, makevalues(
			std::int32_t(-3),
			std::int32_t(20),
			nullptr,
			3.14f,
			"ab",
			nullptr,
			makedate(2018, 10, 5),
			3.10e-10
		), {
			protocol_field_type::tiny,
			protocol_field_type::short_,
			protocol_field_type::long_,
			protocol_field_type::float_,
			protocol_field_type::string,
			protocol_field_type::long_,
			protocol_field_type::date,
			protocol_field_type::double_
		}
	)
), test_name_generator);

// Error cases for deserialize_binary_row
struct BinaryRowErrorParam : named_param
{
	std::string name;
	std::vector<std::uint8_t> from;
	Error expected;
	std::vector<protocol_field_type> types;

	BinaryRowErrorParam(
		std::string name,
		std::vector<std::uint8_t> from,
		Error expected,
		std::vector<protocol_field_type> types
	):
		name(std::move(name)),
		from(std::move(from)),
		expected(expected),
		types(std::move(types))
	{
	}
};

struct DeserializeBinaryRowErrorTest : public TestWithParam<BinaryRowErrorParam> {};

TEST_P(DeserializeBinaryRowErrorTest, ErrorCondition_ReturnsErrorCode)
{
	auto meta = make_meta(GetParam().types);
	const auto& buffer = GetParam().from;
	DeserializationContext ctx (buffer.data(), buffer.data() + buffer.size(), capabilities());

	std::vector<value> actual;
	auto err = deserialize_binary_row(ctx, meta, actual);
	EXPECT_EQ(err, make_error_code(GetParam().expected));
}

INSTANTIATE_TEST_SUITE_P(Default, DeserializeBinaryRowErrorTest, testing::Values(
	BinaryRowErrorParam("no_space_null_bitmap_1", {0x00}, Error::incomplete_message, {protocol_field_type::tiny}),
	BinaryRowErrorParam("no_space_null_bitmap_2", {0x00, 0xfc}, Error::incomplete_message,
			std::vector<protocol_field_type>(7, protocol_field_type::tiny)),
	BinaryRowErrorParam("no_space_value_single", {0x00, 0x00}, Error::incomplete_message, {protocol_field_type::tiny}),
	BinaryRowErrorParam("no_space_value_last", {0x00, 0x00, 0x01}, Error::incomplete_message,
			std::vector<protocol_field_type>(2, protocol_field_type::tiny)),
	BinaryRowErrorParam("no_space_value_middle", {0x00, 0x00, 0x01}, Error::incomplete_message,
			std::vector<protocol_field_type>(3, protocol_field_type::tiny)),
	BinaryRowErrorParam("extra_bytes", {0x00, 0x00, 0x01, 0x02}, Error::extra_bytes, {protocol_field_type::tiny})
), test_name_generator);


} // anon namespace

