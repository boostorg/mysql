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

// for deserializa_binary_value
struct BinaryValueParam
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

std::ostream& operator<<(std::ostream& os, const BinaryValueParam& value) { return os << value.name; }

struct DeserializeBinaryValueTest : public TestWithParam<BinaryValueParam>
{
};

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
));

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
));

INSTANTIATE_TEST_SUITE_P(FloatingPointTypes, DeserializeBinaryValueTest, Values(
	BinaryValueParam("float", {0x66, 0x66, 0x86, 0xc0}, -4.2f, protocol_field_type::float_),
	BinaryValueParam("double", {0xcd, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x10, 0xc0}, -4.2, protocol_field_type::double_)
));

INSTANTIATE_TEST_SUITE_P(TimeTypes, DeserializeBinaryValueTest, Values(
	BinaryValueParam("date", {0x04, 0xda, 0x07, 0x03, 0x1c}, makedate(2010, 3, 28), protocol_field_type::date),
	BinaryValueParam("datetime", {0x0b, 0xda, 0x07, 0x05, 0x02, 0x17, 0x01, 0x32, 0xa0, 0x86, 0x01, 0x00},
			makedt(2010, 5, 2, 23, 1, 50, 100000), protocol_field_type::datetime),
	BinaryValueParam("timestamp", {0x0b, 0xda, 0x07, 0x05, 0x02, 0x17, 0x01, 0x32, 0xa0, 0x86, 0x01, 0x00},
			makedt(2010, 5, 2, 23, 1, 50, 100000), protocol_field_type::timestamp),
	BinaryValueParam("time", {  0x0c, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x02, 0x03, 0xa0, 0x86, 0x01, 0x00},
			maket(120, 2, 3, 100000), protocol_field_type::time),
	BinaryValueParam("year", {0xe3, 0x07}, std::uint32_t(2019), protocol_field_type::year, column_flags::unsigned_)
));

/*struct DeserializeTextRowTest : public Test
{
	std::vector<mysql::field_metadata> meta {
		column_definition_packet {
			string_lenenc("def"),
			string_lenenc("awesome"),
			string_lenenc("test_table"),
			string_lenenc("test_table"),
			string_lenenc("f0"),
			string_lenenc("f0"),
			collation::utf8_general_ci,
			int4(300),
			protocol_field_type::var_string,
			int2(0),
			int1(0)
		},
		column_definition_packet {
			string_lenenc("def"),
			string_lenenc("awesome"),
			string_lenenc("test_table"),
			string_lenenc("test_table"),
			string_lenenc("f1"),
			string_lenenc("f1"),
			collation::binary,
			int4(11),
			protocol_field_type::long_,
			int2(0),
			int1(0)
		},
		column_definition_packet {
			string_lenenc("def"),
			string_lenenc("awesome"),
			string_lenenc("test_table"),
			string_lenenc("test_table"),
			string_lenenc("f2"),
			string_lenenc("f2"),
			collation::binary,
			int4(22),
			protocol_field_type::datetime,
			int2(column_flags::binary),
			int1(2)
		}
	};
	std::vector<value> values;

	error_code deserialize(const std::vector<std::uint8_t>& buffer)
	{
		DeserializationContext ctx (buffer.data(), buffer.data() + buffer.size(), capabilities());
		return deserialize_text_row(ctx, meta, values);
	}
};

TEST_F(DeserializeTextRowTest, SameNumberOfValuesAsFieldsNonNulls_DeserializesReturnsOk)
{
	std::vector<value> expected_values {value("val"), value(std::int32_t(21)), value(makedt(2010, 10, 1))};
	std::vector<std::uint8_t> buffer {
		0x03, 0x76, 0x61, 0x6c, 0x02, 0x32, 0x31, 0x16,
		0x32, 0x30, 0x31, 0x30, 0x2d, 0x31, 0x30, 0x2d,
		0x30, 0x31, 0x20, 0x30, 0x30, 0x3a, 0x30, 0x30,
		0x3a, 0x30, 0x30, 0x2e, 0x30, 0x30
	};
	auto err = deserialize(buffer);
	EXPECT_EQ(err, error_code());
	EXPECT_EQ(values, expected_values);
}

TEST_F(DeserializeTextRowTest, SameNumberOfValuesAsFieldsOneNull_DeserializesReturnsOk)
{
	std::vector<value> expected_values {value("val"), value(nullptr), value(makedt(2010, 10, 1))};
	std::vector<std::uint8_t> buffer {
		0x03, 0x76, 0x61, 0x6c, 0xfb, 0x16, 0x32, 0x30,
		0x31, 0x30, 0x2d, 0x31, 0x30, 0x2d, 0x30, 0x31,
		0x20, 0x30, 0x30, 0x3a, 0x30, 0x30, 0x3a, 0x30,
		0x30, 0x2e, 0x30, 0x30
	};
	auto err = deserialize(buffer);
	EXPECT_EQ(err, error_code());
	EXPECT_EQ(values, expected_values);
}

TEST_F(DeserializeTextRowTest, SameNumberOfValuesAsFieldsAllNull_DeserializesReturnsOk)
{
	std::vector<value> expected_values {value(nullptr), value(nullptr), value(nullptr)};
	auto err = deserialize({0xfb, 0xfb, 0xfb});
	EXPECT_EQ(err, error_code());
	EXPECT_EQ(values, expected_values);
}

TEST_F(DeserializeTextRowTest, TooFewValues_ReturnsError)
{
	auto err = deserialize({0xfb, 0xfb});
	EXPECT_EQ(err, make_error_code(Error::incomplete_message));
}

TEST_F(DeserializeTextRowTest, TooManyValues_ReturnsError)
{
	auto err = deserialize({0xfb, 0xfb, 0xfb, 0xfb});
	EXPECT_EQ(err, make_error_code(Error::extra_bytes));
}

TEST_F(DeserializeTextRowTest, ErrorDeserializingContainerStringValue_ReturnsError)
{
	auto err = deserialize({0x03, 0xaa, 0xab, 0xfb, 0xfb});
	EXPECT_EQ(err, make_error_code(Error::incomplete_message));
}

TEST_F(DeserializeTextRowTest, ErrorDeserializingContainerValue_ReturnsError)
{
	std::vector<std::uint8_t> buffer {
		0x03, 0x76, 0x61, 0x6c, 0xfb, 0x16, 0x32, 0x30,
		0x31, 0x30, 0x2d, 0x31, 0x30, 0x2d, 0x30, 0x31,
		0x20, 0x30, 0x30, 0x3a, 0x30, 0x30, 0x3a, 0x30,
		0x30, 0x2f, 0x30, 0x30
	};
	auto err = deserialize(buffer);
	EXPECT_EQ(err, make_error_code(Error::protocol_value_error));
}*/

}


