//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <gtest/gtest.h>
#include "boost/mysql/detail/protocol/text_deserialization.hpp"
#include "test_common.hpp"

using namespace boost::mysql::detail;
using namespace boost::mysql::test;
using namespace testing;
using namespace date::literals;
using boost::mysql::value;
using boost::mysql::collation;
using boost::mysql::error_code;
using boost::mysql::errc;

namespace
{

using boost::mysql::operator<<;

// Positive cases, single value
struct text_value_testcase : named_param
{
    std::string name;
    std::string_view from;
    value expected;
    protocol_field_type type;
    unsigned decimals;
    std::uint16_t flags;

    template <typename T>
    text_value_testcase(
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

struct DeserializeTextValueTest : public TestWithParam<text_value_testcase> {};

TEST_P(DeserializeTextValueTest, CorrectFormat_SetsOutputValueReturnsTrue)
{
    column_definition_packet coldef;
    coldef.type = GetParam().type;
    coldef.decimals.value = static_cast<std::uint8_t>(GetParam().decimals);
    coldef.flags.value = GetParam().flags;
    boost::mysql::field_metadata meta (coldef);
    value actual_value;
    auto err = deserialize_text_value(GetParam().from, meta, actual_value);
    EXPECT_EQ(err, errc::ok);
    EXPECT_EQ(actual_value, GetParam().expected);
}

INSTANTIATE_TEST_SUITE_P(StringTypes, DeserializeTextValueTest, Values(
    text_value_testcase("varchar_non_empty", "string", "string", protocol_field_type::var_string),
    text_value_testcase("varchar_empty", "", "", protocol_field_type::var_string),
    text_value_testcase("char", "", "", protocol_field_type::string),
    text_value_testcase("varbinary", "value", "value", protocol_field_type::var_string, column_flags::binary),
    text_value_testcase("binary", "value", "value", protocol_field_type::string, column_flags::binary),
    text_value_testcase("text_blob", "value", "value", protocol_field_type::blob, column_flags::blob),
    text_value_testcase("enum", "value", "value", protocol_field_type::string, column_flags::enum_),
    text_value_testcase("set", "value1,value2", "value1,value2", protocol_field_type::string, column_flags::set),

    text_value_testcase("bit", "\1", "\1", protocol_field_type::bit),
    text_value_testcase("decimal", "\1", "\1", protocol_field_type::newdecimal),
    text_value_testcase("geomtry", "\1", "\1", protocol_field_type::geometry, column_flags::binary | column_flags::blob)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(TINYINT, DeserializeTextValueTest, Values(
    text_value_testcase("signed", "20", std::int32_t(20), protocol_field_type::tiny),
    text_value_testcase("signed_max", "127", std::int32_t(127), protocol_field_type::tiny),
    text_value_testcase("signed_negative", "-20", std::int32_t(-20), protocol_field_type::tiny),
    text_value_testcase("signed_negative_max", "-128", std::int32_t(-128), protocol_field_type::tiny),
    text_value_testcase("unsigned", "20", std::uint32_t(20), protocol_field_type::tiny, column_flags::unsigned_),
    text_value_testcase("usigned_min", "0", std::uint32_t(0), protocol_field_type::tiny, column_flags::unsigned_),
    text_value_testcase("usigned_max", "255", std::uint32_t(255), protocol_field_type::tiny, column_flags::unsigned_),
    text_value_testcase("usigned_zerofill", "010", std::uint32_t(10), protocol_field_type::tiny, column_flags::unsigned_)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(SMALLINT, DeserializeTextValueTest, Values(
    text_value_testcase("signed", "20", std::int32_t(20), protocol_field_type::short_),
    text_value_testcase("signed_max", "32767", std::int32_t(32767), protocol_field_type::short_),
    text_value_testcase("signed_negative", "-20", std::int32_t(-20), protocol_field_type::short_),
    text_value_testcase("signed_negative_max", "-32768", std::int32_t(-32768), protocol_field_type::short_),
    text_value_testcase("unsigned", "20", std::uint32_t(20), protocol_field_type::short_, column_flags::unsigned_),
    text_value_testcase("usigned_min", "0", std::uint32_t(0), protocol_field_type::short_, column_flags::unsigned_),
    text_value_testcase("usigned_max", "65535", std::uint32_t(65535), protocol_field_type::short_, column_flags::unsigned_),
    text_value_testcase("usigned_zerofill", "00535", std::uint32_t(535), protocol_field_type::short_, column_flags::unsigned_)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(MEDIUMINT, DeserializeTextValueTest, Values(
    text_value_testcase("signed", "20", std::int32_t(20), protocol_field_type::int24),
    text_value_testcase("signed_max", "8388607", std::int32_t(8388607), protocol_field_type::int24),
    text_value_testcase("signed_negative", "-20", std::int32_t(-20), protocol_field_type::int24),
    text_value_testcase("signed_negative_max", "-8388607", std::int32_t(-8388607), protocol_field_type::int24),
    text_value_testcase("unsigned", "20", std::uint32_t(20), protocol_field_type::int24, column_flags::unsigned_),
    text_value_testcase("usigned_min", "0", std::uint32_t(0), protocol_field_type::int24, column_flags::unsigned_),
    text_value_testcase("usigned_max", "16777215", std::uint32_t(16777215), protocol_field_type::int24, column_flags::unsigned_),
    text_value_testcase("usigned_zerofill", "00007215", std::uint32_t(7215), protocol_field_type::int24, column_flags::unsigned_)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(INT, DeserializeTextValueTest, Values(
    text_value_testcase("signed", "20", std::int32_t(20), protocol_field_type::long_),
    text_value_testcase("signed_max", "2147483647", std::int32_t(2147483647), protocol_field_type::long_),
    text_value_testcase("signed_negative", "-20", std::int32_t(-20), protocol_field_type::long_),
    text_value_testcase("signed_negative_max", "-2147483648", std::int32_t(-2147483648), protocol_field_type::long_),
    text_value_testcase("unsigned", "20", std::uint32_t(20), protocol_field_type::long_, column_flags::unsigned_),
    text_value_testcase("usigned_min", "0", std::uint32_t(0), protocol_field_type::long_, column_flags::unsigned_),
    text_value_testcase("usigned_max", "4294967295", std::uint32_t(4294967295), protocol_field_type::long_, column_flags::unsigned_),
    text_value_testcase("usigned_zerofill", "0000067295", std::uint32_t(67295), protocol_field_type::long_, column_flags::unsigned_)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(BIGINT, DeserializeTextValueTest, Values(
    text_value_testcase("signed", "20", std::int64_t(20), protocol_field_type::longlong),
    text_value_testcase("signed_max", "9223372036854775807", std::int64_t(9223372036854775807), protocol_field_type::longlong),
    text_value_testcase("signed_negative", "-20", std::int64_t(-20), protocol_field_type::longlong),
    text_value_testcase("signed_negative max", "-9223372036854775808", std::numeric_limits<std::int64_t>::min(), protocol_field_type::longlong),
    text_value_testcase("unsigned", "20", std::uint64_t(20), protocol_field_type::longlong, column_flags::unsigned_),
    text_value_testcase("usigned_min", "0", std::uint64_t(0), protocol_field_type::longlong, column_flags::unsigned_),
    text_value_testcase("usigned_max", "18446744073709551615", std::uint64_t(18446744073709551615ULL), protocol_field_type::longlong, column_flags::unsigned_),
    text_value_testcase("usigned_zerofill", "000615", std::uint64_t(615), protocol_field_type::longlong, column_flags::unsigned_)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(FLOAT, DeserializeTextValueTest, Values(
    text_value_testcase("zero", "0", 0.0f, protocol_field_type::float_),
    text_value_testcase("integer_positive", "4", 4.0f, protocol_field_type::float_),
    text_value_testcase("integer_negative", "-5", -5.0f, protocol_field_type::float_),
    text_value_testcase("fractional_positive", "3.147", 3.147f, protocol_field_type::float_),
    text_value_testcase("fractional_negative", "-3.147", -3.147f, protocol_field_type::float_),
    text_value_testcase("positive_exponent_positive_integer", "3e20", 3e20f, protocol_field_type::float_),
    text_value_testcase("positive_exponent_negative_integer", "-3e20", -3e20f, protocol_field_type::float_),
    text_value_testcase("positive_exponent_positive_fractional", "3.14e20", 3.14e20f, protocol_field_type::float_),
    text_value_testcase("positive_exponent_negative_fractional", "-3.45e20", -3.45e20f, protocol_field_type::float_),
    text_value_testcase("negative_exponent_positive_integer", "3e-20", 3e-20f, protocol_field_type::float_),
    text_value_testcase("negative_exponent_negative_integer", "-3e-20", -3e-20f, protocol_field_type::float_),
    text_value_testcase("negative_exponent_positive_fractional", "3.14e-20", 3.14e-20f, protocol_field_type::float_),
    text_value_testcase("negative_exponent_negative_fractional", "-3.45e-20", -3.45e-20f, protocol_field_type::float_)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(DOUBLE, DeserializeTextValueTest, Values(
    text_value_testcase("zero", "0", 0.0, protocol_field_type::double_),
    text_value_testcase("integer_positive", "4", 4.0, protocol_field_type::double_),
    text_value_testcase("integer_negative", "-5", -5.0, protocol_field_type::double_),
    text_value_testcase("fractional_positive", "3.147", 3.147, protocol_field_type::double_),
    text_value_testcase("fractional_negative", "-3.147", -3.147, protocol_field_type::double_),
    text_value_testcase("positive_exponent_positive_integer", "3e20", 3e20, protocol_field_type::double_),
    text_value_testcase("positive_exponent_negative_integer", "-3e20", -3e20, protocol_field_type::double_),
    text_value_testcase("positive_exponent_positive_fractional", "3.14e20", 3.14e20, protocol_field_type::double_),
    text_value_testcase("positive_exponent_negative_fractional", "-3.45e20", -3.45e20, protocol_field_type::double_),
    text_value_testcase("negative_exponent_positive_integer", "3e-20", 3e-20, protocol_field_type::double_),
    text_value_testcase("negative_exponent_negative_integer", "-3e-20", -3e-20, protocol_field_type::double_),
    text_value_testcase("negative_exponent_positive_fractional", "3.14e-20", 3.14e-20, protocol_field_type::double_),
    text_value_testcase("negative_exponent_negative_fractional", "-3.45e-20", -3.45e-20, protocol_field_type::double_)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(DATE, DeserializeTextValueTest, Values(
    text_value_testcase("regular_date", "2019-02-28", boost::mysql::date(2019_y/2/28), protocol_field_type::date),
    text_value_testcase("leap_year", "1788-02-29", boost::mysql::date(1788_y/2/29), protocol_field_type::date),
    text_value_testcase("min", "1000-01-01", boost::mysql::date(1000_y/1/1), protocol_field_type::date),
    text_value_testcase("max", "9999-12-31", boost::mysql::date(9999_y/12/31), protocol_field_type::date),
    text_value_testcase("unofficial_min", "0100-01-01", boost::mysql::date(100_y/1/1), protocol_field_type::date)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(DATETIME, DeserializeTextValueTest, Values(
    text_value_testcase("0_decimals_date", "2010-02-15 00:00:00", makedt(2010, 2, 15), protocol_field_type::datetime),
    text_value_testcase("0_decimals_h", "2010-02-15 02:00:00", makedt(2010, 2, 15, 2), protocol_field_type::datetime),
    text_value_testcase("0_decimals_hm", "2010-02-15 02:05:00", makedt(2010, 2, 15, 2, 5), protocol_field_type::datetime),
    text_value_testcase("0_decimals_hms", "2010-02-15 02:05:30", makedt(2010, 2, 15, 2, 5, 30), protocol_field_type::datetime),
    text_value_testcase("0_decimals_min", "1000-01-01 00:00:00", makedt(1000, 1, 1), protocol_field_type::datetime),
    text_value_testcase("0_decimals_max", "9999-12-31 23:59:59", makedt(9999, 12, 31, 23, 59, 59), protocol_field_type::datetime),

    text_value_testcase("1_decimals_date", "2010-02-15 00:00:00.0", makedt(2010, 2, 15), protocol_field_type::datetime, 0, 1),
    text_value_testcase("1_decimals_h", "2010-02-15 02:00:00.0", makedt(2010, 2, 15, 2), protocol_field_type::datetime, 0, 1),
    text_value_testcase("1_decimals_hm", "2010-02-15 02:05:00.0", makedt(2010, 2, 15, 2, 5), protocol_field_type::datetime, 0, 1),
    text_value_testcase("1_decimals_hms", "2010-02-15 02:05:30.0", makedt(2010, 2, 15, 2, 5, 30), protocol_field_type::datetime, 0, 1),
    text_value_testcase("1_decimals_hmsu", "2010-02-15 02:05:30.5", makedt(2010, 2, 15, 2, 5, 30, 500000), protocol_field_type::datetime, 0, 1),
    text_value_testcase("1_decimals_min", "1000-01-01 00:00:00.0", makedt(1000, 1, 1), protocol_field_type::datetime, 0, 1),
    text_value_testcase("1_decimals_max", "9999-12-31 23:59:59.9", makedt(9999, 12, 31, 23, 59, 59, 900000), protocol_field_type::datetime, 0, 1),

    text_value_testcase("2_decimals_hms", "2010-02-15 02:05:30.00", makedt(2010, 2, 15, 2, 5, 30), protocol_field_type::datetime, 0, 2),
    text_value_testcase("2_decimals_hmsu", "2010-02-15 02:05:30.05", makedt(2010, 2, 15, 2, 5, 30, 50000), protocol_field_type::datetime, 0, 2),
    text_value_testcase("2_decimals_min", "1000-01-01 00:00:00.00", makedt(1000, 1, 1), protocol_field_type::datetime, 0, 2),
    text_value_testcase("2_decimals_max", "9999-12-31 23:59:59.99", makedt(9999, 12, 31, 23, 59, 59, 990000), protocol_field_type::datetime, 0, 2),

    text_value_testcase("3_decimals_hms", "2010-02-15 02:05:30.000", makedt(2010, 2, 15, 2, 5, 30), protocol_field_type::datetime, 0, 3),
    text_value_testcase("3_decimals_hmsu", "2010-02-15 02:05:30.420", makedt(2010, 2, 15, 2, 5, 30, 420000), protocol_field_type::datetime, 0, 3),
    text_value_testcase("3_decimals_min", "1000-01-01 00:00:00.000", makedt(1000, 1, 1), protocol_field_type::datetime, 0, 3),
    text_value_testcase("3_decimals_max", "9999-12-31 23:59:59.999", makedt(9999, 12, 31, 23, 59, 59, 999000), protocol_field_type::datetime, 0, 3),

    text_value_testcase("4_decimals_hms", "2010-02-15 02:05:30.0000", makedt(2010, 2, 15, 2, 5, 30), protocol_field_type::datetime, 0, 4),
    text_value_testcase("4_decimals_hmsu", "2010-02-15 02:05:30.4267", makedt(2010, 2, 15, 2, 5, 30, 426700), protocol_field_type::datetime, 0, 4),
    text_value_testcase("4_decimals_min", "1000-01-01 00:00:00.0000", makedt(1000, 1, 1), protocol_field_type::datetime, 0, 4),
    text_value_testcase("4_decimals_max", "9999-12-31 23:59:59.9999", makedt(9999, 12, 31, 23, 59, 59, 999900), protocol_field_type::datetime, 0, 4),

    text_value_testcase("5_decimals_hms", "2010-02-15 02:05:30.00000", makedt(2010, 2, 15, 2, 5, 30), protocol_field_type::datetime, 0, 5),
    text_value_testcase("5_decimals_hmsu", "2010-02-15 02:05:30.00239", makedt(2010, 2, 15, 2, 5, 30, 2390), protocol_field_type::datetime, 0, 5),
    text_value_testcase("5_decimals_min", "1000-01-01 00:00:00.00000", makedt(1000, 1, 1), protocol_field_type::datetime, 0, 5),
    text_value_testcase("5_decimals_max", "9999-12-31 23:59:59.99999", makedt(9999, 12, 31, 23, 59, 59, 999990), protocol_field_type::datetime, 0, 5),

    text_value_testcase("6_decimals_hms", "2010-02-15 02:05:30.000000", makedt(2010, 2, 15, 2, 5, 30), protocol_field_type::datetime, 0, 6),
    text_value_testcase("6_decimals_hmsu", "2010-02-15 02:05:30.002395", makedt(2010, 2, 15, 2, 5, 30, 2395), protocol_field_type::datetime, 0, 6),
    text_value_testcase("6_decimals_min", "1000-01-01 00:00:00.000000", makedt(1000, 1, 1), protocol_field_type::datetime, 0, 6),
    text_value_testcase("6_decimals_max", "9999-12-31 23:59:59.999999", makedt(9999, 12, 31, 23, 59, 59, 999999), protocol_field_type::datetime, 0, 6)
), test_name_generator);

// Right now, timestamps are deserialized as DATETIMEs. TODO: update this when we consider time zones
INSTANTIATE_TEST_SUITE_P(TIMESTAMP, DeserializeTextValueTest, Values(
    text_value_testcase("0_decimals", "2010-02-15 02:05:30", makedt(2010, 2, 15, 2, 5, 30), protocol_field_type::timestamp),
    text_value_testcase("6_decimals", "2010-02-15 02:05:30.085670", makedt(2010, 2, 15, 2, 5, 30, 85670), protocol_field_type::timestamp, 0, 6),
    text_value_testcase("6_decimals_min", "1970-01-01 00:00:01.000000", makedt(1970, 1, 1, 0, 0, 1), protocol_field_type::timestamp, 0, 6),
    text_value_testcase("6_decimals_max", "2038-01-19 03:14:07.999999", makedt(2038, 1, 19, 3, 14, 7, 999999), protocol_field_type::timestamp, 0, 6)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(TIME, DeserializeTextValueTest, Values(
    text_value_testcase("0_decimals_positive_h", "01:00:00", maket(1, 0, 0), protocol_field_type::time),
    text_value_testcase("0_decimals_positive_hm", "12:03:00", maket(12, 3, 0), protocol_field_type::time),
    text_value_testcase("0_decimals_positive_hms", "14:51:23", maket(14, 51, 23), protocol_field_type::time),
    text_value_testcase("0_decimals_max", "838:59:59", maket(838, 59, 59), protocol_field_type::time),
    text_value_testcase("0_decimals_negative_h", "-06:00:00", -maket(6, 0, 0), protocol_field_type::time),
    text_value_testcase("0_decimals_negative_hm", "-12:03:00", -maket(12, 3, 0), protocol_field_type::time),
    text_value_testcase("0_decimals_negative_hms", "-14:51:23", -maket(14, 51, 23), protocol_field_type::time),
    text_value_testcase("0_decimals_min", "-838:59:59", -maket(838, 59, 59), protocol_field_type::time),
    text_value_testcase("0_decimals_zero", "00:00:00", maket(0, 0, 0), protocol_field_type::time),

    text_value_testcase("1_decimals_positive_hms", "14:51:23.0", maket(14, 51, 23), protocol_field_type::time, 0, 1),
    text_value_testcase("1_decimals_positive_hmsu", "14:51:23.5", maket(14, 51, 23, 500000), protocol_field_type::time, 0, 1),
    text_value_testcase("1_decimals_max", "838:59:58.9", maket(838, 59, 58, 900000), protocol_field_type::time, 0, 1),
    text_value_testcase("1_decimals_negative_hms", "-14:51:23.0", -maket(14, 51, 23), protocol_field_type::time, 0, 1),
    text_value_testcase("1_decimals_negative_hmsu", "-14:51:23.5", -maket(14, 51, 23, 500000), protocol_field_type::time, 0, 1),
    text_value_testcase("1_decimals_min", "-838:59:58.9", -maket(838, 59, 58, 900000), protocol_field_type::time, 0, 1),
    text_value_testcase("1_decimals_zero", "00:00:00.0", maket(0, 0, 0), protocol_field_type::time, 0, 1),

    text_value_testcase("2_decimals_positive_hms", "14:51:23.00", maket(14, 51, 23), protocol_field_type::time, 0, 2),
    text_value_testcase("2_decimals_positive_hmsu", "14:51:23.52", maket(14, 51, 23, 520000), protocol_field_type::time, 0, 2),
    text_value_testcase("2_decimals_max", "838:59:58.99", maket(838, 59, 58, 990000), protocol_field_type::time, 0, 2),
    text_value_testcase("2_decimals_negative_hms", "-14:51:23.00", -maket(14, 51, 23), protocol_field_type::time, 0, 2),
    text_value_testcase("2_decimals_negative_hmsu", "-14:51:23.50", -maket(14, 51, 23, 500000), protocol_field_type::time, 0, 2),
    text_value_testcase("2_decimals_min", "-838:59:58.99", -maket(838, 59, 58, 990000), protocol_field_type::time, 0, 2),
    text_value_testcase("2_decimals_zero", "00:00:00.00", maket(0, 0, 0), protocol_field_type::time, 0, 2),

    text_value_testcase("3_decimals_positive_hms", "14:51:23.000", maket(14, 51, 23), protocol_field_type::time, 0, 3),
    text_value_testcase("3_decimals_positive_hmsu", "14:51:23.501", maket(14, 51, 23, 501000), protocol_field_type::time, 0, 3),
    text_value_testcase("3_decimals_max", "838:59:58.999", maket(838, 59, 58, 999000), protocol_field_type::time, 0, 3),
    text_value_testcase("3_decimals_negative_hms", "-14:51:23.000", -maket(14, 51, 23), protocol_field_type::time, 0, 3),
    text_value_testcase("3_decimals_negative_hmsu", "-14:51:23.003", -maket(14, 51, 23, 3000), protocol_field_type::time, 0, 3),
    text_value_testcase("3_decimals_min", "-838:59:58.999", -maket(838, 59, 58, 999000), protocol_field_type::time, 0, 3),
    text_value_testcase("3_decimals_zero", "00:00:00.000", maket(0, 0, 0), protocol_field_type::time, 0, 3),

    text_value_testcase("4_decimals_positive_hms", "14:51:23.0000", maket(14, 51, 23), protocol_field_type::time, 0, 4),
    text_value_testcase("4_decimals_positive_hmsu", "14:51:23.5017", maket(14, 51, 23, 501700), protocol_field_type::time, 0, 4),
    text_value_testcase("4_decimals_max", "838:59:58.9999", maket(838, 59, 58, 999900), protocol_field_type::time, 0, 4),
    text_value_testcase("4_decimals_negative_hms", "-14:51:23.0000", -maket(14, 51, 23), protocol_field_type::time, 0, 4),
    text_value_testcase("4_decimals_negative_hmsu", "-14:51:23.0038", -maket(14, 51, 23, 3800), protocol_field_type::time, 0, 4),
    text_value_testcase("4_decimals_min", "-838:59:58.9999", -maket(838, 59, 58, 999900), protocol_field_type::time, 0, 4),
    text_value_testcase("4_decimals_zero", "00:00:00.0000", maket(0, 0, 0), protocol_field_type::time, 0, 4),

    text_value_testcase("5_decimals_positive_hms", "14:51:23.00000", maket(14, 51, 23), protocol_field_type::time, 0, 5),
    text_value_testcase("5_decimals_positive_hmsu", "14:51:23.50171", maket(14, 51, 23, 501710), protocol_field_type::time, 0, 5),
    text_value_testcase("5_decimals_max", "838:59:58.99999", maket(838, 59, 58, 999990), protocol_field_type::time, 0, 5),
    text_value_testcase("5_decimals_negative_hms", "-14:51:23.00000", -maket(14, 51, 23), protocol_field_type::time, 0, 5),
    text_value_testcase("5_decimals_negative_hmsu", "-14:51:23.00009", -maket(14, 51, 23, 90), protocol_field_type::time, 0, 5),
    text_value_testcase("5_decimals_min", "-838:59:58.99999", -maket(838, 59, 58, 999990), protocol_field_type::time, 0, 5),
    text_value_testcase("5_decimals_zero", "00:00:00.00000", maket(0, 0, 0), protocol_field_type::time, 0, 5),

    text_value_testcase("6_decimals_positive_hms", "14:51:23.000000", maket(14, 51, 23), protocol_field_type::time, 0, 6),
    text_value_testcase("6_decimals_positive_hmsu", "14:51:23.501717", maket(14, 51, 23, 501717), protocol_field_type::time, 0, 6),
    text_value_testcase("6_decimals_max", "838:59:58.999999", maket(838, 59, 58, 999999), protocol_field_type::time, 0, 6),
    text_value_testcase("6_decimals_negative_hms", "-14:51:23.000000", -maket(14, 51, 23), protocol_field_type::time, 0, 6),
    text_value_testcase("6_decimals_negative_hmsu", "-14:51:23.900000", -maket(14, 51, 23, 900000), protocol_field_type::time, 0, 6),
    text_value_testcase("6_decimals_min", "-838:59:58.999999", -maket(838, 59, 58, 999999), protocol_field_type::time, 0, 6),
    text_value_testcase("6_decimals_zero", "00:00:00.000000", maket(0, 0, 0), protocol_field_type::time, 0, 6)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(YEAR, DeserializeTextValueTest, Values(
    text_value_testcase("regular_value", "1999", std::uint32_t(1999), protocol_field_type::year, column_flags::unsigned_),
    text_value_testcase("min", "1901", std::uint32_t(1901), protocol_field_type::year, column_flags::unsigned_),
    text_value_testcase("max", "2155", std::uint32_t(2155), protocol_field_type::year, column_flags::unsigned_),
    text_value_testcase("zero", "0000", std::uint32_t(0), protocol_field_type::year, column_flags::unsigned_)
), test_name_generator);

// Negative cases, value
struct err_text_value_testcase : named_param
{
    std::string name;
    std::string_view from;
    protocol_field_type type;
    unsigned decimals;
    std::uint16_t flags;
    errc expected_err;

    err_text_value_testcase(std::string&& name, std::string_view from, protocol_field_type type,
            unsigned decimals=0, std::uint16_t flags=0, errc expected_err=errc::protocol_value_error) :
        name(std::move(name)),
        from(from),
        type(type),
        decimals(decimals),
        flags(flags),
        expected_err(expected_err)
    {
    }
};

struct DeserializeTextValueErrorTest : TestWithParam<err_text_value_testcase>
{
};

TEST_P(DeserializeTextValueErrorTest, Error_ReturnsExpectedErrc)
{
    column_definition_packet coldef;
    coldef.type = GetParam().type;
    coldef.decimals.value = static_cast<std::uint8_t>(GetParam().decimals);
    coldef.flags.value = GetParam().flags;
    boost::mysql::field_metadata meta (coldef);
    value actual_value;
    auto err = deserialize_text_value(GetParam().from, meta, actual_value);
    auto expected = GetParam().expected_err;
    EXPECT_EQ(expected, err)
        << "expected: " << error_to_string(expected) << ", actual: " << error_to_string(err);
}

std::vector<err_text_value_testcase> make_int_err_cases(
    protocol_field_type t,
    std::string_view signed_lt_min,
    std::string_view signed_gt_max,
    std::string_view unsigned_lt_min,
    std::string_view unsigned_gt_max
)
{
    return {
        err_text_value_testcase("signed_blank", "", t),
        err_text_value_testcase("signed_non_number", "abtrf", t),
        err_text_value_testcase("signed_hex", "0x01", t),
        err_text_value_testcase("signed_fractional", "1.1", t),
        err_text_value_testcase("signed_exp", "2e10", t),
        err_text_value_testcase("signed_lt_min", signed_lt_min, t),
        err_text_value_testcase("signed_gt_max", signed_gt_max, t),
        err_text_value_testcase("unsigned_blank", "", t, column_flags::unsigned_),
        err_text_value_testcase("unsigned_non_number", "abtrf", t, column_flags::unsigned_),
        err_text_value_testcase("unsigned_hex", "0x01", t, column_flags::unsigned_),
        err_text_value_testcase("unsigned_fractional", "1.1", t, column_flags::unsigned_),
        err_text_value_testcase("unsigned_exp", "2e10", t, column_flags::unsigned_),
        err_text_value_testcase("unsigned_lt_min", unsigned_lt_min, t, column_flags::unsigned_),
        err_text_value_testcase("unsigned_gt_max", unsigned_gt_max, t, column_flags::unsigned_)
    };
}

std::vector<err_text_value_testcase> make_int32_err_cases(
    protocol_field_type t
)
{
    // Unsigned integers behaviour for negative inputs are determined by what iostreams
    // do (accepting it and overflowing)
    return make_int_err_cases(t, "-2147483649", "2147483648", "-2147483649", "4294967296");
}

INSTANTIATE_TEST_SUITE_P(TINYINT, DeserializeTextValueErrorTest, ValuesIn(
    make_int32_err_cases(protocol_field_type::tiny)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(SMALLINT, DeserializeTextValueErrorTest, ValuesIn(
    make_int32_err_cases(protocol_field_type::short_)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(MEDIUMINT, DeserializeTextValueErrorTest, ValuesIn(
    make_int32_err_cases(protocol_field_type::int24)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(INT, DeserializeTextValueErrorTest, ValuesIn(
    make_int32_err_cases(protocol_field_type::long_)
), test_name_generator);

std::vector<err_text_value_testcase> make_int64_err_cases(
    protocol_field_type t
)
{
    // Unsigned integers behaviour for negative inputs are determined by what iostreams
    // do (accepting it and overflowing)
    return make_int_err_cases(
        t,
        "-9223372036854775809",
        "9223372036854775808",
        "-9223372036854775809",
        "18446744073709551616"
    );
}

INSTANTIATE_TEST_SUITE_P(BIGINT, DeserializeTextValueErrorTest, ValuesIn(
    make_int64_err_cases(protocol_field_type::longlong)
), test_name_generator);

std::vector<err_text_value_testcase> make_float_err_cases(
    protocol_field_type t,
    std::string_view lt_min,
    std::string_view gt_max
)
{
    return {
        err_text_value_testcase("blank", "", t),
        err_text_value_testcase("non_number", "abtrf", t),
        err_text_value_testcase("hex", "0x01", t),
        err_text_value_testcase("lt_min", lt_min, t),
        err_text_value_testcase("gt_max", gt_max, t),
        err_text_value_testcase("inf", "inf", t), // inf values not allowed by SQL std
        err_text_value_testcase("minus_inf", "-inf", t),
        err_text_value_testcase("nan", "nan", t), // nan values not allowed by SQL std
        err_text_value_testcase("minus_nan", "-nan", t)
    };
}

INSTANTIATE_TEST_SUITE_P(FLOAT, DeserializeTextValueErrorTest, ValuesIn(
    make_float_err_cases(protocol_field_type::float_, "-2e90", "2e90")
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(DOUBLE, DeserializeTextValueErrorTest, ValuesIn(
    make_float_err_cases(protocol_field_type::double_, "-2e9999", "2e9999")
), test_name_generator);

// All cases, row
struct DeserializeTextRowTest : public Test
{
    std::vector<boost::mysql::field_metadata> meta {
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
        deserialization_context ctx (buffer.data(), buffer.data() + buffer.size(), capabilities());
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
    EXPECT_EQ(err, make_error_code(errc::incomplete_message));
}

TEST_F(DeserializeTextRowTest, TooManyValues_ReturnsError)
{
    auto err = deserialize({0xfb, 0xfb, 0xfb, 0xfb});
    EXPECT_EQ(err, make_error_code(errc::extra_bytes));
}

TEST_F(DeserializeTextRowTest, ErrorDeserializingContainerStringValue_ReturnsError)
{
    auto err = deserialize({0x03, 0xaa, 0xab, 0xfb, 0xfb});
    EXPECT_EQ(err, make_error_code(errc::incomplete_message));
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
    EXPECT_EQ(err, make_error_code(errc::protocol_value_error));
}

}


