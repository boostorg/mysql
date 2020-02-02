/*
 * deserialize_row.cpp
 *
 *  Created on: Nov 8, 2019
 *      Author: ruben
 */

#include <gtest/gtest.h>
#include "mysql/impl/text_deserialization.hpp"
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

struct TextValueParam : named_param
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
	}
};

struct DeserializeTextValueTest : public TestWithParam<TextValueParam> {};

TEST_P(DeserializeTextValueTest, CorrectFormat_SetsOutputValueReturnsTrue)
{
	column_definition_packet coldef;
	coldef.type = GetParam().type;
	coldef.decimals.value = static_cast<std::uint8_t>(GetParam().decimals);
	coldef.flags.value = GetParam().flags;
	mysql::field_metadata meta (coldef);
	value actual_value;
	auto err = deserialize_text_value(GetParam().from, meta, actual_value);
	EXPECT_EQ(err, Error::ok);
	EXPECT_EQ(actual_value, GetParam().expected);
}

INSTANTIATE_TEST_SUITE_P(StringTypes, DeserializeTextValueTest, Values(
	TextValueParam("varchar_non_empty", "string", "string", protocol_field_type::var_string),
	TextValueParam("varchar_empty", "", "", protocol_field_type::var_string),
	TextValueParam("char", "", "", protocol_field_type::string),
	TextValueParam("varbinary", "value", "value", protocol_field_type::var_string, column_flags::binary),
	TextValueParam("binary", "value", "value", protocol_field_type::string, column_flags::binary),
	TextValueParam("text_blob", "value", "value", protocol_field_type::blob, column_flags::blob),
	TextValueParam("enum", "value", "value", protocol_field_type::string, column_flags::enum_),
	TextValueParam("set", "value1,value2", "value1,value2", protocol_field_type::string, column_flags::set),

	TextValueParam("bit", "\1", "\1", protocol_field_type::bit),
	TextValueParam("decimal", "\1", "\1", protocol_field_type::newdecimal),
	TextValueParam("geomtry", "\1", "\1", protocol_field_type::geometry, column_flags::binary | column_flags::blob)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(TINYINT, DeserializeTextValueTest, Values(
	TextValueParam("signed", "20", std::int32_t(20), protocol_field_type::tiny),
	TextValueParam("signed_max", "127", std::int32_t(127), protocol_field_type::tiny),
	TextValueParam("signed_negative", "-20", std::int32_t(-20), protocol_field_type::tiny),
	TextValueParam("signed_negative_max", "-128", std::int32_t(-128), protocol_field_type::tiny),
	TextValueParam("unsigned", "20", std::uint32_t(20), protocol_field_type::tiny, column_flags::unsigned_),
	TextValueParam("usigned_min", "0", std::uint32_t(0), protocol_field_type::tiny, column_flags::unsigned_),
	TextValueParam("usigned_max", "255", std::uint32_t(255), protocol_field_type::tiny, column_flags::unsigned_),
	TextValueParam("usigned_zerofill", "010", std::uint32_t(10), protocol_field_type::tiny, column_flags::unsigned_)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(SMALLINT, DeserializeTextValueTest, Values(
	TextValueParam("signed", "20", std::int32_t(20), protocol_field_type::short_),
	TextValueParam("signed_max", "32767", std::int32_t(32767), protocol_field_type::short_),
	TextValueParam("signed_negative", "-20", std::int32_t(-20), protocol_field_type::short_),
	TextValueParam("signed_negative_max", "-32768", std::int32_t(-32768), protocol_field_type::short_),
	TextValueParam("unsigned", "20", std::uint32_t(20), protocol_field_type::short_, column_flags::unsigned_),
	TextValueParam("usigned_min", "0", std::uint32_t(0), protocol_field_type::short_, column_flags::unsigned_),
	TextValueParam("usigned_max", "65535", std::uint32_t(65535), protocol_field_type::short_, column_flags::unsigned_),
	TextValueParam("usigned_zerofill", "00535", std::uint32_t(535), protocol_field_type::short_, column_flags::unsigned_)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(MEDIUMINT, DeserializeTextValueTest, Values(
	TextValueParam("signed", "20", std::int32_t(20), protocol_field_type::int24),
	TextValueParam("signed_max", "8388607", std::int32_t(8388607), protocol_field_type::int24),
	TextValueParam("signed_negative", "-20", std::int32_t(-20), protocol_field_type::int24),
	TextValueParam("signed_negative_max", "-8388607", std::int32_t(-8388607), protocol_field_type::int24),
	TextValueParam("unsigned", "20", std::uint32_t(20), protocol_field_type::int24, column_flags::unsigned_),
	TextValueParam("usigned_min", "0", std::uint32_t(0), protocol_field_type::int24, column_flags::unsigned_),
	TextValueParam("usigned_max", "16777215", std::uint32_t(16777215), protocol_field_type::int24, column_flags::unsigned_),
	TextValueParam("usigned_zerofill", "00007215", std::uint32_t(7215), protocol_field_type::int24, column_flags::unsigned_)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(INT, DeserializeTextValueTest, Values(
	TextValueParam("signed", "20", std::int32_t(20), protocol_field_type::long_),
	TextValueParam("signed_max", "2147483647", std::int32_t(2147483647), protocol_field_type::long_),
	TextValueParam("signed_negative", "-20", std::int32_t(-20), protocol_field_type::long_),
	TextValueParam("signed_negative_max", "-2147483648", std::int32_t(-2147483648), protocol_field_type::long_),
	TextValueParam("unsigned", "20", std::uint32_t(20), protocol_field_type::long_, column_flags::unsigned_),
	TextValueParam("usigned_min", "0", std::uint32_t(0), protocol_field_type::long_, column_flags::unsigned_),
	TextValueParam("usigned_max", "4294967295", std::uint32_t(4294967295), protocol_field_type::long_, column_flags::unsigned_),
	TextValueParam("usigned_zerofill", "0000067295", std::uint32_t(67295), protocol_field_type::long_, column_flags::unsigned_)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(BIGINT, DeserializeTextValueTest, Values(
	TextValueParam("signed", "20", std::int64_t(20), protocol_field_type::longlong),
	TextValueParam("signed_max", "9223372036854775807", std::int64_t(9223372036854775807), protocol_field_type::longlong),
	TextValueParam("signed_negative", "-20", std::int64_t(-20), protocol_field_type::longlong),
	TextValueParam("signed_negative max", "-9223372036854775808", std::numeric_limits<std::int64_t>::min(), protocol_field_type::longlong),
	TextValueParam("unsigned", "20", std::uint64_t(20), protocol_field_type::longlong, column_flags::unsigned_),
	TextValueParam("usigned_min", "0", std::uint64_t(0), protocol_field_type::longlong, column_flags::unsigned_),
	TextValueParam("usigned_max", "18446744073709551615", std::uint64_t(18446744073709551615ULL), protocol_field_type::longlong, column_flags::unsigned_),
	TextValueParam("usigned_zerofill", "000615", std::uint64_t(615), protocol_field_type::longlong, column_flags::unsigned_)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(FLOAT, DeserializeTextValueTest, Values(
	TextValueParam("zero", "0", 0.0f, protocol_field_type::float_),
	TextValueParam("integer_positive", "4", 4.0f, protocol_field_type::float_),
	TextValueParam("integer_negative", "-5", -5.0f, protocol_field_type::float_),
	TextValueParam("fractional_positive", "3.147", 3.147f, protocol_field_type::float_),
	TextValueParam("fractional_negative", "-3.147", -3.147f, protocol_field_type::float_),
	TextValueParam("positive_exponent_positive_integer", "3e20", 3e20f, protocol_field_type::float_),
	TextValueParam("positive_exponent_negative_integer", "-3e20", -3e20f, protocol_field_type::float_),
	TextValueParam("positive_exponent_positive_fractional", "3.14e20", 3.14e20f, protocol_field_type::float_),
	TextValueParam("positive_exponent_negative_fractional", "-3.45e20", -3.45e20f, protocol_field_type::float_),
	TextValueParam("negative_exponent_positive_integer", "3e-20", 3e-20f, protocol_field_type::float_),
	TextValueParam("negative_exponent_negative_integer", "-3e-20", -3e-20f, protocol_field_type::float_),
	TextValueParam("negative_exponent_positive_fractional", "3.14e-20", 3.14e-20f, protocol_field_type::float_),
	TextValueParam("negative_exponent_negative_fractional", "-3.45e-20", -3.45e-20f, protocol_field_type::float_)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(DOUBLE, DeserializeTextValueTest, Values(
	TextValueParam("zero", "0", 0.0, protocol_field_type::double_),
	TextValueParam("integer_positive", "4", 4.0, protocol_field_type::double_),
	TextValueParam("integer_negative", "-5", -5.0, protocol_field_type::double_),
	TextValueParam("fractional_positive", "3.147", 3.147, protocol_field_type::double_),
	TextValueParam("fractional_negative", "-3.147", -3.147, protocol_field_type::double_),
	TextValueParam("positive_exponent_positive_integer", "3e20", 3e20, protocol_field_type::double_),
	TextValueParam("positive_exponent_negative_integer", "-3e20", -3e20, protocol_field_type::double_),
	TextValueParam("positive_exponent_positive_fractional", "3.14e20", 3.14e20, protocol_field_type::double_),
	TextValueParam("positive_exponent_negative_fractional", "-3.45e20", -3.45e20, protocol_field_type::double_),
	TextValueParam("negative_exponent_positive_integer", "3e-20", 3e-20, protocol_field_type::double_),
	TextValueParam("negative_exponent_negative_integer", "-3e-20", -3e-20, protocol_field_type::double_),
	TextValueParam("negative_exponent_positive_fractional", "3.14e-20", 3.14e-20, protocol_field_type::double_),
	TextValueParam("negative_exponent_negative_fractional", "-3.45e-20", -3.45e-20, protocol_field_type::double_)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(DATE, DeserializeTextValueTest, Values(
	TextValueParam("regular_date", "2019-02-28", mysql::date(2019_y/2/28), protocol_field_type::date),
	TextValueParam("leap_year", "1788-02-29", mysql::date(1788_y/2/29), protocol_field_type::date),
	TextValueParam("min", "1000-01-01", mysql::date(1000_y/1/1), protocol_field_type::date),
	TextValueParam("max", "9999-12-31", mysql::date(9999_y/12/31), protocol_field_type::date),
	TextValueParam("unofficial_min", "0100-01-01", mysql::date(100_y/1/1), protocol_field_type::date)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(DATETIME, DeserializeTextValueTest, Values(
	TextValueParam("0_decimals_date", "2010-02-15 00:00:00", makedt(2010, 2, 15), protocol_field_type::datetime),
	TextValueParam("0_decimals_h", "2010-02-15 02:00:00", makedt(2010, 2, 15, 2), protocol_field_type::datetime),
	TextValueParam("0_decimals_hm", "2010-02-15 02:05:00", makedt(2010, 2, 15, 2, 5), protocol_field_type::datetime),
	TextValueParam("0_decimals_hms", "2010-02-15 02:05:30", makedt(2010, 2, 15, 2, 5, 30), protocol_field_type::datetime),
	TextValueParam("0_decimals_min", "1000-01-01 00:00:00", makedt(1000, 1, 1), protocol_field_type::datetime),
	TextValueParam("0_decimals_max", "9999-12-31 23:59:59", makedt(9999, 12, 31, 23, 59, 59), protocol_field_type::datetime),

	TextValueParam("1_decimals_date", "2010-02-15 00:00:00.0", makedt(2010, 2, 15), protocol_field_type::datetime, 0, 1),
	TextValueParam("1_decimals_h", "2010-02-15 02:00:00.0", makedt(2010, 2, 15, 2), protocol_field_type::datetime, 0, 1),
	TextValueParam("1_decimals_hm", "2010-02-15 02:05:00.0", makedt(2010, 2, 15, 2, 5), protocol_field_type::datetime, 0, 1),
	TextValueParam("1_decimals_hms", "2010-02-15 02:05:30.0", makedt(2010, 2, 15, 2, 5, 30), protocol_field_type::datetime, 0, 1),
	TextValueParam("1_decimals_hmsu", "2010-02-15 02:05:30.5", makedt(2010, 2, 15, 2, 5, 30, 500000), protocol_field_type::datetime, 0, 1),
	TextValueParam("1_decimals_min", "1000-01-01 00:00:00.0", makedt(1000, 1, 1), protocol_field_type::datetime, 0, 1),
	TextValueParam("1_decimals_max", "9999-12-31 23:59:59.9", makedt(9999, 12, 31, 23, 59, 59, 900000), protocol_field_type::datetime, 0, 1),

	TextValueParam("2_decimals_hms", "2010-02-15 02:05:30.00", makedt(2010, 2, 15, 2, 5, 30), protocol_field_type::datetime, 0, 2),
	TextValueParam("2_decimals_hmsu", "2010-02-15 02:05:30.05", makedt(2010, 2, 15, 2, 5, 30, 50000), protocol_field_type::datetime, 0, 2),
	TextValueParam("2_decimals_min", "1000-01-01 00:00:00.00", makedt(1000, 1, 1), protocol_field_type::datetime, 0, 2),
	TextValueParam("2_decimals_max", "9999-12-31 23:59:59.99", makedt(9999, 12, 31, 23, 59, 59, 990000), protocol_field_type::datetime, 0, 2),

	TextValueParam("3_decimals_hms", "2010-02-15 02:05:30.000", makedt(2010, 2, 15, 2, 5, 30), protocol_field_type::datetime, 0, 3),
	TextValueParam("3_decimals_hmsu", "2010-02-15 02:05:30.420", makedt(2010, 2, 15, 2, 5, 30, 420000), protocol_field_type::datetime, 0, 3),
	TextValueParam("3_decimals_min", "1000-01-01 00:00:00.000", makedt(1000, 1, 1), protocol_field_type::datetime, 0, 3),
	TextValueParam("3_decimals_max", "9999-12-31 23:59:59.999", makedt(9999, 12, 31, 23, 59, 59, 999000), protocol_field_type::datetime, 0, 3),

	TextValueParam("4_decimals_hms", "2010-02-15 02:05:30.0000", makedt(2010, 2, 15, 2, 5, 30), protocol_field_type::datetime, 0, 4),
	TextValueParam("4_decimals_hmsu", "2010-02-15 02:05:30.4267", makedt(2010, 2, 15, 2, 5, 30, 426700), protocol_field_type::datetime, 0, 4),
	TextValueParam("4_decimals_min", "1000-01-01 00:00:00.0000", makedt(1000, 1, 1), protocol_field_type::datetime, 0, 4),
	TextValueParam("4_decimals_max", "9999-12-31 23:59:59.9999", makedt(9999, 12, 31, 23, 59, 59, 999900), protocol_field_type::datetime, 0, 4),

	TextValueParam("5_decimals_hms", "2010-02-15 02:05:30.00000", makedt(2010, 2, 15, 2, 5, 30), protocol_field_type::datetime, 0, 5),
	TextValueParam("5_decimals_hmsu", "2010-02-15 02:05:30.00239", makedt(2010, 2, 15, 2, 5, 30, 2390), protocol_field_type::datetime, 0, 5),
	TextValueParam("5_decimals_min", "1000-01-01 00:00:00.00000", makedt(1000, 1, 1), protocol_field_type::datetime, 0, 5),
	TextValueParam("5_decimals_max", "9999-12-31 23:59:59.99999", makedt(9999, 12, 31, 23, 59, 59, 999990), protocol_field_type::datetime, 0, 5),

	TextValueParam("6_decimals_hms", "2010-02-15 02:05:30.000000", makedt(2010, 2, 15, 2, 5, 30), protocol_field_type::datetime, 0, 6),
	TextValueParam("6_decimals_hmsu", "2010-02-15 02:05:30.002395", makedt(2010, 2, 15, 2, 5, 30, 2395), protocol_field_type::datetime, 0, 6),
	TextValueParam("6_decimals_min", "1000-01-01 00:00:00.000000", makedt(1000, 1, 1), protocol_field_type::datetime, 0, 6),
	TextValueParam("6_decimals_max", "9999-12-31 23:59:59.999999", makedt(9999, 12, 31, 23, 59, 59, 999999), protocol_field_type::datetime, 0, 6)
), test_name_generator);

// Right now, timestamps are deserialized as DATETIMEs. TODO: update this when we consider time zones
INSTANTIATE_TEST_SUITE_P(TIMESTAMP, DeserializeTextValueTest, Values(
	TextValueParam("0_decimals", "2010-02-15 02:05:30", makedt(2010, 2, 15, 2, 5, 30), protocol_field_type::timestamp),
	TextValueParam("6_decimals", "2010-02-15 02:05:30.085670", makedt(2010, 2, 15, 2, 5, 30, 85670), protocol_field_type::timestamp, 0, 6),
	TextValueParam("6_decimals_min", "1970-01-01 00:00:01.000000", makedt(1970, 1, 1, 0, 0, 1), protocol_field_type::timestamp, 0, 6),
	TextValueParam("6_decimals_max", "2038-01-19 03:14:07.999999", makedt(2038, 1, 19, 3, 14, 7, 999999), protocol_field_type::timestamp, 0, 6)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(TIME, DeserializeTextValueTest, Values(
	TextValueParam("0_decimals_positive_h", "01:00:00", maket(1, 0, 0), protocol_field_type::time),
	TextValueParam("0_decimals_positive_hm", "12:03:00", maket(12, 3, 0), protocol_field_type::time),
	TextValueParam("0_decimals_positive_hms", "14:51:23", maket(14, 51, 23), protocol_field_type::time),
	TextValueParam("0_decimals_max", "838:59:59", maket(838, 59, 59), protocol_field_type::time),
	TextValueParam("0_decimals_negative_h", "-06:00:00", -maket(6, 0, 0), protocol_field_type::time),
	TextValueParam("0_decimals_negative_hm", "-12:03:00", -maket(12, 3, 0), protocol_field_type::time),
	TextValueParam("0_decimals_negative_hms", "-14:51:23", -maket(14, 51, 23), protocol_field_type::time),
	TextValueParam("0_decimals_min", "-838:59:59", -maket(838, 59, 59), protocol_field_type::time),
	TextValueParam("0_decimals_zero", "00:00:00", maket(0, 0, 0), protocol_field_type::time),

	TextValueParam("1_decimals_positive_hms", "14:51:23.0", maket(14, 51, 23), protocol_field_type::time, 0, 1),
	TextValueParam("1_decimals_positive_hmsu", "14:51:23.5", maket(14, 51, 23, 500000), protocol_field_type::time, 0, 1),
	TextValueParam("1_decimals_max", "838:59:58.9", maket(838, 59, 58, 900000), protocol_field_type::time, 0, 1),
	TextValueParam("1_decimals_negative_hms", "-14:51:23.0", -maket(14, 51, 23), protocol_field_type::time, 0, 1),
	TextValueParam("1_decimals_negative_hmsu", "-14:51:23.5", -maket(14, 51, 23, 500000), protocol_field_type::time, 0, 1),
	TextValueParam("1_decimals_min", "-838:59:58.9", -maket(838, 59, 58, 900000), protocol_field_type::time, 0, 1),
	TextValueParam("1_decimals_zero", "00:00:00.0", maket(0, 0, 0), protocol_field_type::time, 0, 1),

	TextValueParam("2_decimals_positive_hms", "14:51:23.00", maket(14, 51, 23), protocol_field_type::time, 0, 2),
	TextValueParam("2_decimals_positive_hmsu", "14:51:23.52", maket(14, 51, 23, 520000), protocol_field_type::time, 0, 2),
	TextValueParam("2_decimals_max", "838:59:58.99", maket(838, 59, 58, 990000), protocol_field_type::time, 0, 2),
	TextValueParam("2_decimals_negative_hms", "-14:51:23.00", -maket(14, 51, 23), protocol_field_type::time, 0, 2),
	TextValueParam("2_decimals_negative_hmsu", "-14:51:23.50", -maket(14, 51, 23, 500000), protocol_field_type::time, 0, 2),
	TextValueParam("2_decimals_min", "-838:59:58.99", -maket(838, 59, 58, 990000), protocol_field_type::time, 0, 2),
	TextValueParam("2_decimals_zero", "00:00:00.00", maket(0, 0, 0), protocol_field_type::time, 0, 2),

	TextValueParam("3_decimals_positive_hms", "14:51:23.000", maket(14, 51, 23), protocol_field_type::time, 0, 3),
	TextValueParam("3_decimals_positive_hmsu", "14:51:23.501", maket(14, 51, 23, 501000), protocol_field_type::time, 0, 3),
	TextValueParam("3_decimals_max", "838:59:58.999", maket(838, 59, 58, 999000), protocol_field_type::time, 0, 3),
	TextValueParam("3_decimals_negative_hms", "-14:51:23.000", -maket(14, 51, 23), protocol_field_type::time, 0, 3),
	TextValueParam("3_decimals_negative_hmsu", "-14:51:23.003", -maket(14, 51, 23, 3000), protocol_field_type::time, 0, 3),
	TextValueParam("3_decimals_min", "-838:59:58.999", -maket(838, 59, 58, 999000), protocol_field_type::time, 0, 3),
	TextValueParam("3_decimals_zero", "00:00:00.000", maket(0, 0, 0), protocol_field_type::time, 0, 3),

	TextValueParam("4_decimals_positive_hms", "14:51:23.0000", maket(14, 51, 23), protocol_field_type::time, 0, 4),
	TextValueParam("4_decimals_positive_hmsu", "14:51:23.5017", maket(14, 51, 23, 501700), protocol_field_type::time, 0, 4),
	TextValueParam("4_decimals_max", "838:59:58.9999", maket(838, 59, 58, 999900), protocol_field_type::time, 0, 4),
	TextValueParam("4_decimals_negative_hms", "-14:51:23.0000", -maket(14, 51, 23), protocol_field_type::time, 0, 4),
	TextValueParam("4_decimals_negative_hmsu", "-14:51:23.0038", -maket(14, 51, 23, 3800), protocol_field_type::time, 0, 4),
	TextValueParam("4_decimals_min", "-838:59:58.9999", -maket(838, 59, 58, 999900), protocol_field_type::time, 0, 4),
	TextValueParam("4_decimals_zero", "00:00:00.0000", maket(0, 0, 0), protocol_field_type::time, 0, 4),

	TextValueParam("5_decimals_positive_hms", "14:51:23.00000", maket(14, 51, 23), protocol_field_type::time, 0, 5),
	TextValueParam("5_decimals_positive_hmsu", "14:51:23.50171", maket(14, 51, 23, 501710), protocol_field_type::time, 0, 5),
	TextValueParam("5_decimals_max", "838:59:58.99999", maket(838, 59, 58, 999990), protocol_field_type::time, 0, 5),
	TextValueParam("5_decimals_negative_hms", "-14:51:23.00000", -maket(14, 51, 23), protocol_field_type::time, 0, 5),
	TextValueParam("5_decimals_negative_hmsu", "-14:51:23.00009", -maket(14, 51, 23, 90), protocol_field_type::time, 0, 5),
	TextValueParam("5_decimals_min", "-838:59:58.99999", -maket(838, 59, 58, 999990), protocol_field_type::time, 0, 5),
	TextValueParam("5_decimals_zero", "00:00:00.00000", maket(0, 0, 0), protocol_field_type::time, 0, 5),

	TextValueParam("6_decimals_positive_hms", "14:51:23.000000", maket(14, 51, 23), protocol_field_type::time, 0, 6),
	TextValueParam("6_decimals_positive_hmsu", "14:51:23.501717", maket(14, 51, 23, 501717), protocol_field_type::time, 0, 6),
	TextValueParam("6_decimals_max", "838:59:58.999999", maket(838, 59, 58, 999999), protocol_field_type::time, 0, 6),
	TextValueParam("6_decimals_negative_hms", "-14:51:23.000000", -maket(14, 51, 23), protocol_field_type::time, 0, 6),
	TextValueParam("6_decimals_negative_hmsu", "-14:51:23.900000", -maket(14, 51, 23, 900000), protocol_field_type::time, 0, 6),
	TextValueParam("6_decimals_min", "-838:59:58.999999", -maket(838, 59, 58, 999999), protocol_field_type::time, 0, 6),
	TextValueParam("6_decimals_zero", "00:00:00.000000", maket(0, 0, 0), protocol_field_type::time, 0, 6)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(YEAR, DeserializeTextValueTest, Values(
	TextValueParam("regular_value", "1999", std::uint32_t(1999), protocol_field_type::year, column_flags::unsigned_),
	TextValueParam("min", "1901", std::uint32_t(1901), protocol_field_type::year, column_flags::unsigned_),
	TextValueParam("max", "2155", std::uint32_t(2155), protocol_field_type::year, column_flags::unsigned_),
	TextValueParam("zero", "0000", std::uint32_t(0), protocol_field_type::year, column_flags::unsigned_)
), test_name_generator);

struct DeserializeTextRowTest : public Test
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
}

}


