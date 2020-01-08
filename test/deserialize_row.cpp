/*
 * deserialize_row.cpp
 *
 *  Created on: Nov 8, 2019
 *      Author: ruben
 */

#include <gtest/gtest.h>
#include <boost/type_index.hpp>
#include "mysql/impl/deserialize_row.hpp"
#include "test_common.hpp"

using namespace mysql;
using namespace mysql::detail;
using namespace mysql::test;
using namespace testing;
using namespace ::date::literals;

namespace
{

struct TextValueParam
{
	std::string name;
	std::string_view from;
	value expected;
	protocol_field_type type;
	unsigned decimals;
	std::uint16_t flags;

	template <typename T>
	TextValueParam(
		std::string name,
		std::string_view from,
		T&& expected_value,
		protocol_field_type type,
		std::uint16_t flags=0,
		unsigned decimals=0
	):
		name(std::move(name)),
		from(from),
		expected(std::forward<T>(expected_value)),
		type(type),
		decimals(decimals),
		flags(flags)
	{
	};
};

std::ostream& operator<<(std::ostream& os, const TextValueParam& value) { return os << value.name; }

struct DeserializeTextValueTest : public TestWithParam<TextValueParam>
{
};

TEST_P(DeserializeTextValueTest, CorrectFormat_SetsOutputValueReturnsTrue)
{
	msgs::column_definition coldef;
	coldef.type = GetParam().type;
	coldef.decimals.value = static_cast<std::uint8_t>(GetParam().decimals);
	coldef.flags.value = GetParam().flags;
	field_metadata meta (coldef);
	value actual_value;
	auto err = deserialize_text_value(GetParam().from, meta, actual_value);
	EXPECT_EQ(err, Error::ok);
	EXPECT_EQ(actual_value, GetParam().expected);
}

INSTANTIATE_TEST_SUITE_P(StringTypes, DeserializeTextValueTest, Values(
	TextValueParam("VARCHAR non-empty", "string", "string", protocol_field_type::var_string),
	TextValueParam("VARCHAR empty", "", "", protocol_field_type::var_string),
	TextValueParam("CHAR", "", "", protocol_field_type::string),
	TextValueParam("VARBINARY", "value", "value", protocol_field_type::var_string, column_flags::binary),
	TextValueParam("BINARY", "value", "value", protocol_field_type::string, column_flags::binary),
	TextValueParam("TEXT and BLOB", "value", "value", protocol_field_type::blob, column_flags::blob),
	TextValueParam("ENUM", "value", "value", protocol_field_type::string, column_flags::enum_),
	TextValueParam("SET", "value1,value2", "value1,value2", protocol_field_type::string, column_flags::set),

	TextValueParam("BIT", "\1", "\1", protocol_field_type::bit),
	TextValueParam("DECIMAL", "\1", "\1", protocol_field_type::bit),
	TextValueParam("GEOMTRY", "\1", "\1", protocol_field_type::geometry, column_flags::binary | column_flags::blob)
));

INSTANTIATE_TEST_SUITE_P(TINYINT, DeserializeTextValueTest, Values(
	TextValueParam("signed", "20", std::int32_t(20), protocol_field_type::tiny),
	TextValueParam("signed max", "127", std::int32_t(127), protocol_field_type::tiny),
	TextValueParam("signed negative", "-20", std::int32_t(-20), protocol_field_type::tiny),
	TextValueParam("signed negative max", "-128", std::int32_t(-128), protocol_field_type::tiny),
	TextValueParam("unsigned", "20", std::uint32_t(20), protocol_field_type::tiny, column_flags::unsigned_),
	TextValueParam("usigned min", "0", std::uint32_t(0), protocol_field_type::tiny, column_flags::unsigned_),
	TextValueParam("usigned max", "255", std::uint32_t(255), protocol_field_type::tiny, column_flags::unsigned_),
	TextValueParam("usigned zerofill", "010", std::uint32_t(10), protocol_field_type::tiny, column_flags::unsigned_)
));

INSTANTIATE_TEST_SUITE_P(SMALLINT, DeserializeTextValueTest, Values(
	TextValueParam("signed", "20", std::int32_t(20), protocol_field_type::short_),
	TextValueParam("signed max", "32767", std::int32_t(32767), protocol_field_type::short_),
	TextValueParam("signed negative", "-20", std::int32_t(-20), protocol_field_type::short_),
	TextValueParam("signed negative max", "-32768", std::int32_t(-32768), protocol_field_type::short_),
	TextValueParam("unsigned", "20", std::uint32_t(20), protocol_field_type::short_, column_flags::unsigned_),
	TextValueParam("usigned min", "0", std::uint32_t(0), protocol_field_type::short_, column_flags::unsigned_),
	TextValueParam("usigned max", "65535", std::uint32_t(65535), protocol_field_type::short_, column_flags::unsigned_),
	TextValueParam("usigned zerofill", "00535", std::uint32_t(535), protocol_field_type::short_, column_flags::unsigned_)
));

INSTANTIATE_TEST_SUITE_P(MEDIUMINT, DeserializeTextValueTest, Values(
	TextValueParam("signed", "20", std::int32_t(20), protocol_field_type::int24),
	TextValueParam("signed max", "8388607", std::int32_t(8388607), protocol_field_type::int24),
	TextValueParam("signed negative", "-20", std::int32_t(-20), protocol_field_type::int24),
	TextValueParam("signed negative max", "-8388607", std::int32_t(-8388607), protocol_field_type::int24),
	TextValueParam("unsigned", "20", std::uint32_t(20), protocol_field_type::int24, column_flags::unsigned_),
	TextValueParam("usigned min", "0", std::uint32_t(0), protocol_field_type::int24, column_flags::unsigned_),
	TextValueParam("usigned max", "16777215", std::uint32_t(16777215), protocol_field_type::int24, column_flags::unsigned_),
	TextValueParam("usigned zerofill", "00007215", std::uint32_t(7215), protocol_field_type::int24, column_flags::unsigned_)
));

INSTANTIATE_TEST_SUITE_P(INT, DeserializeTextValueTest, Values(
	TextValueParam("signed", "20", std::int32_t(20), protocol_field_type::long_),
	TextValueParam("signed max", "2147483647", std::int32_t(2147483647), protocol_field_type::long_),
	TextValueParam("signed negative", "-20", std::int32_t(-20), protocol_field_type::long_),
	TextValueParam("signed negative max", "-2147483648", std::int32_t(-2147483648), protocol_field_type::long_),
	TextValueParam("unsigned", "20", std::uint32_t(20), protocol_field_type::long_, column_flags::unsigned_),
	TextValueParam("usigned min", "0", std::uint32_t(0), protocol_field_type::long_, column_flags::unsigned_),
	TextValueParam("usigned max", "4294967295", std::uint32_t(4294967295), protocol_field_type::long_, column_flags::unsigned_),
	TextValueParam("usigned zerofill", "0000067295", std::uint32_t(67295), protocol_field_type::long_, column_flags::unsigned_)
));

INSTANTIATE_TEST_SUITE_P(BIGINT, DeserializeTextValueTest, Values(
	TextValueParam("signed", "20", std::int64_t(20), protocol_field_type::longlong),
	TextValueParam("signed max", "9223372036854775807", std::int64_t(9223372036854775807), protocol_field_type::longlong),
	TextValueParam("signed negative", "-20", std::int64_t(-20), protocol_field_type::longlong),
	TextValueParam("signed negative max", "-9223372036854775808", std::numeric_limits<std::int64_t>::min(), protocol_field_type::longlong),
	TextValueParam("unsigned", "20", std::uint64_t(20), protocol_field_type::longlong, column_flags::unsigned_),
	TextValueParam("usigned min", "0", std::uint64_t(0), protocol_field_type::longlong, column_flags::unsigned_),
	TextValueParam("usigned max", "18446744073709551615", std::uint64_t(18446744073709551615ULL), protocol_field_type::longlong, column_flags::unsigned_),
	TextValueParam("usigned max", "000615", std::uint64_t(615), protocol_field_type::longlong, column_flags::unsigned_)
));

INSTANTIATE_TEST_SUITE_P(FLOAT, DeserializeTextValueTest, Values(
	TextValueParam("zero", "0", 0.0f, protocol_field_type::float_),
	TextValueParam("integer positive", "4", 4.0f, protocol_field_type::float_),
	TextValueParam("integer negative", "-5", -5.0f, protocol_field_type::float_),
	TextValueParam("fractional positive", "3.147", 3.147f, protocol_field_type::float_),
	TextValueParam("fractional negative", "-3.147", -3.147f, protocol_field_type::float_),
	TextValueParam("positive exponent positive integer", "3e20", 3e20f, protocol_field_type::float_),
	TextValueParam("positive exponent negative integer", "-3e20", -3e20f, protocol_field_type::float_),
	TextValueParam("positive exponent positive fractional", "3.14e20", 3.14e20f, protocol_field_type::float_),
	TextValueParam("positive exponent negative fractional", "-3.45e20", -3.45e20f, protocol_field_type::float_),
	TextValueParam("negative exponent positive integer", "3e-20", 3e-20f, protocol_field_type::float_),
	TextValueParam("negative exponent negative integer", "-3e-20", -3e-20f, protocol_field_type::float_),
	TextValueParam("negative exponent positive fractional", "3.14e-20", 3.14e-20f, protocol_field_type::float_),
	TextValueParam("negative exponent negative fractional", "-3.45e-20", -3.45e-20f, protocol_field_type::float_)
));

INSTANTIATE_TEST_SUITE_P(DOUBLE, DeserializeTextValueTest, Values(
	TextValueParam("zero", "0", 0.0, protocol_field_type::double_),
	TextValueParam("integer positive", "4", 4.0, protocol_field_type::double_),
	TextValueParam("integer negative", "-5", -5.0, protocol_field_type::double_),
	TextValueParam("fractional positive", "3.147", 3.147, protocol_field_type::double_),
	TextValueParam("fractional negative", "-3.147", -3.147, protocol_field_type::double_),
	TextValueParam("positive exponent positive integer", "3e20", 3e20, protocol_field_type::double_),
	TextValueParam("positive exponent negative integer", "-3e20", -3e20, protocol_field_type::double_),
	TextValueParam("positive exponent positive fractional", "3.14e20", 3.14e20, protocol_field_type::double_),
	TextValueParam("positive exponent negative fractional", "-3.45e20", -3.45e20, protocol_field_type::double_),
	TextValueParam("negative exponent positive integer", "3e-20", 3e-20, protocol_field_type::double_),
	TextValueParam("negative exponent negative integer", "-3e-20", -3e-20, protocol_field_type::double_),
	TextValueParam("negative exponent positive fractional", "3.14e-20", 3.14e-20, protocol_field_type::double_),
	TextValueParam("negative exponent negative fractional", "-3.45e-20", -3.45e-20, protocol_field_type::double_)
));

INSTANTIATE_TEST_SUITE_P(DATE, DeserializeTextValueTest, Values(
	TextValueParam("regular date", "2019-02-28", mysql::date(2019_y/2/28), protocol_field_type::date),
	TextValueParam("leap year", "1788-02-29", mysql::date(1788_y/2/29), protocol_field_type::date),
	TextValueParam("min", "1000-01-01", mysql::date(1000_y/1/1), protocol_field_type::date),
	TextValueParam("max", "9999-12-31", mysql::date(9999_y/12/31), protocol_field_type::date),
	TextValueParam("unofficial min", "0100-01-01", mysql::date(100_y/1/1), protocol_field_type::date)
));

INSTANTIATE_TEST_SUITE_P(DATETIME, DeserializeTextValueTest, Values(
	TextValueParam("0 decimals, only date", "2010-02-15 00:00:00", makedt(2010, 2, 15), protocol_field_type::datetime),
	TextValueParam("0 decimals, date, h", "2010-02-15 02:00:00", makedt(2010, 2, 15, 2), protocol_field_type::datetime),
	TextValueParam("0 decimals, date, hm", "2010-02-15 02:05:00", makedt(2010, 2, 15, 2, 5), protocol_field_type::datetime),
	TextValueParam("0 decimals, date, hms", "2010-02-15 02:05:30", makedt(2010, 2, 15, 2, 5, 30), protocol_field_type::datetime),
	TextValueParam("0 decimals, min", "1000-01-01 00:00:00", makedt(1000, 1, 1), protocol_field_type::datetime),
	TextValueParam("0 decimals, max", "9999-12-31 23:59:59", makedt(9999, 12, 31, 23, 59, 59), protocol_field_type::datetime),

	TextValueParam("1 decimals, only date", "2010-02-15 00:00:00.0", makedt(2010, 2, 15), protocol_field_type::datetime, 0, 1),
	TextValueParam("1 decimals, date, h", "2010-02-15 02:00:00.0", makedt(2010, 2, 15, 2), protocol_field_type::datetime, 0, 1),
	TextValueParam("1 decimals, date, hm", "2010-02-15 02:05:00.0", makedt(2010, 2, 15, 2, 5), protocol_field_type::datetime, 0, 1),
	TextValueParam("1 decimals, date, hms", "2010-02-15 02:05:30.0", makedt(2010, 2, 15, 2, 5, 30), protocol_field_type::datetime, 0, 1),
	TextValueParam("1 decimals, date, hmsu", "2010-02-15 02:05:30.5", makedt(2010, 2, 15, 2, 5, 30, 500000), protocol_field_type::datetime, 0, 1),
	TextValueParam("1 decimals, min", "1000-01-01 00:00:00.0", makedt(1000, 1, 1), protocol_field_type::datetime, 0, 1),
	TextValueParam("1 decimals, max", "9999-12-31 23:59:59.9", makedt(9999, 12, 31, 23, 59, 59, 900000), protocol_field_type::datetime, 0, 1),

	TextValueParam("2 decimals, date, hms", "2010-02-15 02:05:30.00", makedt(2010, 2, 15, 2, 5, 30), protocol_field_type::datetime, 0, 2),
	TextValueParam("2 decimals, date, hmsu", "2010-02-15 02:05:30.05", makedt(2010, 2, 15, 2, 5, 30, 50000), protocol_field_type::datetime, 0, 2),
	TextValueParam("2 decimals, min", "1000-01-01 00:00:00.00", makedt(1000, 1, 1), protocol_field_type::datetime, 0, 2),
	TextValueParam("2 decimals, max", "9999-12-31 23:59:59.99", makedt(9999, 12, 31, 23, 59, 59, 990000), protocol_field_type::datetime, 0, 2),

	TextValueParam("3 decimals, date, hms", "2010-02-15 02:05:30.000", makedt(2010, 2, 15, 2, 5, 30), protocol_field_type::datetime, 0, 3),
	TextValueParam("3 decimals, date, hmsu", "2010-02-15 02:05:30.420", makedt(2010, 2, 15, 2, 5, 30, 420000), protocol_field_type::datetime, 0, 3),
	TextValueParam("3 decimals, min", "1000-01-01 00:00:00.000", makedt(1000, 1, 1), protocol_field_type::datetime, 0, 3),
	TextValueParam("3 decimals, max", "9999-12-31 23:59:59.999", makedt(9999, 12, 31, 23, 59, 59, 999000), protocol_field_type::datetime, 0, 3),

	TextValueParam("4 decimals, date, hms", "2010-02-15 02:05:30.0000", makedt(2010, 2, 15, 2, 5, 30), protocol_field_type::datetime, 0, 4),
	TextValueParam("4 decimals, date, hmsu", "2010-02-15 02:05:30.4267", makedt(2010, 2, 15, 2, 5, 30, 426700), protocol_field_type::datetime, 0, 4),
	TextValueParam("4 decimals, min", "1000-01-01 00:00:00.0000", makedt(1000, 1, 1), protocol_field_type::datetime, 0, 4),
	TextValueParam("4 decimals, max", "9999-12-31 23:59:59.9999", makedt(9999, 12, 31, 23, 59, 59, 999900), protocol_field_type::datetime, 0, 4),

	TextValueParam("5 decimals, date, hms", "2010-02-15 02:05:30.00000", makedt(2010, 2, 15, 2, 5, 30), protocol_field_type::datetime, 0, 5),
	TextValueParam("5 decimals, date, hmsu", "2010-02-15 02:05:30.00239", makedt(2010, 2, 15, 2, 5, 30, 2390), protocol_field_type::datetime, 0, 5),
	TextValueParam("5 decimals, min", "1000-01-01 00:00:00.00000", makedt(1000, 1, 1), protocol_field_type::datetime, 0, 5),
	TextValueParam("5 decimals, max", "9999-12-31 23:59:59.99999", makedt(9999, 12, 31, 23, 59, 59, 999990), protocol_field_type::datetime, 0, 5),

	TextValueParam("6 decimals, date, hms", "2010-02-15 02:05:30.000000", makedt(2010, 2, 15, 2, 5, 30), protocol_field_type::datetime, 0, 6),
	TextValueParam("6 decimals, date, hmsu", "2010-02-15 02:05:30.002395", makedt(2010, 2, 15, 2, 5, 30, 2395), protocol_field_type::datetime, 0, 6),
	TextValueParam("6 decimals, min", "1000-01-01 00:00:00.000000", makedt(1000, 1, 1), protocol_field_type::datetime, 0, 6),
	TextValueParam("6 decimals, max", "9999-12-31 23:59:59.999999", makedt(9999, 12, 31, 23, 59, 59, 999999), protocol_field_type::datetime, 0, 6)
));

// Right now, timestamps are deserialized as DATETIMEs. TODO: update this when we consider time zones
INSTANTIATE_TEST_SUITE_P(TIMESTAMP, DeserializeTextValueTest, Values(
	TextValueParam("0 decimals", "2010-02-15 02:05:30", makedt(2010, 2, 15, 2, 5, 30), protocol_field_type::timestamp),
	TextValueParam("6 decimals", "2010-02-15 02:05:30.085670", makedt(2010, 2, 15, 2, 5, 30, 85670), protocol_field_type::timestamp, 0, 6),
	TextValueParam("6 decimals, min", "1970-01-01 00:00:01.000000", makedt(1970, 1, 1, 0, 0, 1), protocol_field_type::timestamp, 0, 6),
	TextValueParam("6 decimals, max", "2038-01-19 03:14:07.999999", makedt(2038, 1, 19, 3, 14, 7, 999999), protocol_field_type::timestamp, 0, 6)
));

INSTANTIATE_TEST_SUITE_P(TIME, DeserializeTextValueTest, Values(
	TextValueParam("0 decimals, positive h", "01:00:00", maket(1, 0, 0), protocol_field_type::time),
	TextValueParam("0 decimals, positive hm", "12:03:00", maket(12, 3, 0), protocol_field_type::time),
	TextValueParam("0 decimals, positive hms", "14:51:23", maket(14, 51, 23), protocol_field_type::time),
	TextValueParam("0 decimals, max", "838:59:59", maket(838, 59, 59), protocol_field_type::time),
	TextValueParam("0 decimals, negative h", "-06:00:00", -maket(6, 0, 0), protocol_field_type::time),
	TextValueParam("0 decimals, negative hm", "-12:03:00", -maket(12, 3, 0), protocol_field_type::time),
	TextValueParam("0 decimals, negative hms", "-14:51:23", -maket(14, 51, 23), protocol_field_type::time),
	TextValueParam("0 decimals, min", "-838:59:59", -maket(838, 59, 59), protocol_field_type::time),
	TextValueParam("0 decimals, zero", "00:00:00", maket(0, 0, 0), protocol_field_type::time),

	TextValueParam("1 decimals, positive hms", "14:51:23.0", maket(14, 51, 23), protocol_field_type::time, 0, 1),
	TextValueParam("1 decimals, positive hmsu", "14:51:23.5", maket(14, 51, 23, 500000), protocol_field_type::time, 0, 1),
	TextValueParam("1 decimals, max", "838:59:58.9", maket(838, 59, 58, 900000), protocol_field_type::time, 0, 1),
	TextValueParam("1 decimals, negative hms", "-14:51:23.0", -maket(14, 51, 23), protocol_field_type::time, 0, 1),
	TextValueParam("1 decimals, negative hmsu", "-14:51:23.5", -maket(14, 51, 23, 500000), protocol_field_type::time, 0, 1),
	TextValueParam("1 decimals, min", "-838:59:58.9", -maket(838, 59, 58, 900000), protocol_field_type::time, 0, 1),
	TextValueParam("1 decimals, zero", "00:00:00.0", maket(0, 0, 0), protocol_field_type::time, 0, 1),

	TextValueParam("2 decimals, positive hms", "14:51:23.00", maket(14, 51, 23), protocol_field_type::time, 0, 2),
	TextValueParam("2 decimals, positive hmsu", "14:51:23.52", maket(14, 51, 23, 520000), protocol_field_type::time, 0, 2),
	TextValueParam("2 decimals, max", "838:59:58.99", maket(838, 59, 58, 990000), protocol_field_type::time, 0, 2),
	TextValueParam("2 decimals, negative hms", "-14:51:23.00", -maket(14, 51, 23), protocol_field_type::time, 0, 2),
	TextValueParam("2 decimals, negative hmsu", "-14:51:23.50", -maket(14, 51, 23, 500000), protocol_field_type::time, 0, 2),
	TextValueParam("2 decimals, min", "-838:59:58.99", -maket(838, 59, 58, 990000), protocol_field_type::time, 0, 2),
	TextValueParam("2 decimals, zero", "00:00:00.00", maket(0, 0, 0), protocol_field_type::time, 0, 2),

	TextValueParam("3 decimals, positive hms", "14:51:23.000", maket(14, 51, 23), protocol_field_type::time, 0, 3),
	TextValueParam("3 decimals, positive hmsu", "14:51:23.501", maket(14, 51, 23, 501000), protocol_field_type::time, 0, 3),
	TextValueParam("3 decimals, max", "838:59:58.999", maket(838, 59, 58, 999000), protocol_field_type::time, 0, 3),
	TextValueParam("3 decimals, negative hms", "-14:51:23.000", -maket(14, 51, 23), protocol_field_type::time, 0, 3),
	TextValueParam("3 decimals, negative hmsu", "-14:51:23.003", -maket(14, 51, 23, 3000), protocol_field_type::time, 0, 3),
	TextValueParam("3 decimals, min", "-838:59:58.999", -maket(838, 59, 58, 999000), protocol_field_type::time, 0, 3),
	TextValueParam("3 decimals, zero", "00:00:00.000", maket(0, 0, 0), protocol_field_type::time, 0, 3),

	TextValueParam("4 decimals, positive hms", "14:51:23.0000", maket(14, 51, 23), protocol_field_type::time, 0, 4),
	TextValueParam("4 decimals, positive hmsu", "14:51:23.5017", maket(14, 51, 23, 501700), protocol_field_type::time, 0, 4),
	TextValueParam("4 decimals, max", "838:59:58.9999", maket(838, 59, 58, 999900), protocol_field_type::time, 0, 4),
	TextValueParam("4 decimals, negative hms", "-14:51:23.0000", -maket(14, 51, 23), protocol_field_type::time, 0, 4),
	TextValueParam("4 decimals, negative hmsu", "-14:51:23.0038", -maket(14, 51, 23, 3800), protocol_field_type::time, 0, 4),
	TextValueParam("4 decimals, min", "-838:59:58.9999", -maket(838, 59, 58, 999900), protocol_field_type::time, 0, 4),
	TextValueParam("4 decimals, zero", "00:00:00.0000", maket(0, 0, 0), protocol_field_type::time, 0, 4),

	TextValueParam("5 decimals, positive hms", "14:51:23.00000", maket(14, 51, 23), protocol_field_type::time, 0, 5),
	TextValueParam("5 decimals, positive hmsu", "14:51:23.50171", maket(14, 51, 23, 501710), protocol_field_type::time, 0, 5),
	TextValueParam("5 decimals, max", "838:59:58.99999", maket(838, 59, 58, 999990), protocol_field_type::time, 0, 5),
	TextValueParam("5 decimals, negative hms", "-14:51:23.00000", -maket(14, 51, 23), protocol_field_type::time, 0, 5),
	TextValueParam("5 decimals, negative hmsu", "-14:51:23.00009", -maket(14, 51, 23, 90), protocol_field_type::time, 0, 5),
	TextValueParam("5 decimals, min", "-838:59:58.99999", -maket(838, 59, 58, 999990), protocol_field_type::time, 0, 5),
	TextValueParam("5 decimals, zero", "00:00:00.00000", maket(0, 0, 0), protocol_field_type::time, 0, 5),

	TextValueParam("6 decimals, positive hms", "14:51:23.000000", maket(14, 51, 23), protocol_field_type::time, 0, 6),
	TextValueParam("6 decimals, positive hmsu", "14:51:23.501717", maket(14, 51, 23, 501717), protocol_field_type::time, 0, 6),
	TextValueParam("6 decimals, max", "838:59:58.999999", maket(838, 59, 58, 999999), protocol_field_type::time, 0, 6),
	TextValueParam("6 decimals, negative hms", "-14:51:23.000000", -maket(14, 51, 23), protocol_field_type::time, 0, 6),
	TextValueParam("6 decimals, negative hmsu", "-14:51:23.900000", -maket(14, 51, 23, 900000), protocol_field_type::time, 0, 6),
	TextValueParam("6 decimals, min", "-838:59:58.999999", -maket(838, 59, 58, 999999), protocol_field_type::time, 0, 6),
	TextValueParam("6 decimals, zero", "00:00:00.000000", maket(0, 0, 0), protocol_field_type::time, 0, 6)
));

INSTANTIATE_TEST_SUITE_P(YEAR, DeserializeTextValueTest, Values(
	TextValueParam("regular value", "1999", year(1999), protocol_field_type::year),
	TextValueParam("min", "1901", year(1901), protocol_field_type::year),
	TextValueParam("max", "2155", year(2155), protocol_field_type::year),
	TextValueParam("zero", "0000", year(0), protocol_field_type::year)
));

struct DeserializeTextRowTest : public Test
{
	std::vector<field_metadata> meta {
		msgs::column_definition {
			{"def"},
			{"awesome"},
			{"test_table"},
			{"test_table"},
			{"f0"},
			{"f0"},
			collation::utf8_general_ci,
			{300},
			protocol_field_type::var_string,
			{0},
			{0}
		},
		msgs::column_definition {
			{"def"},
			{"awesome"},
			{"test_table"},
			{"test_table"},
			{"f1"},
			{"f1"},
			collation::binary,
			{11},
			protocol_field_type::long_,
			{0},
			{0}
		},
		msgs::column_definition {
			{"def"},
			{"awesome"},
			{"test_table"},
			{"test_table"},
			{"f2"},
			{"f2"},
			collation::binary,
			{22},
			protocol_field_type::datetime,
			{column_flags::binary},
			{2}
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
}

}


