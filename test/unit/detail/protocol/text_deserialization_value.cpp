//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Test deserialize_text_value(), just positive cases

#include <gtest/gtest.h>
#include "boost/mysql/detail/protocol/text_deserialization.hpp"
#include "boost/mysql/detail/auxiliar/stringize.hpp"
#include "test_common.hpp"

using namespace boost::mysql::detail;
using namespace boost::mysql::test;
using namespace testing;
using boost::mysql::value;
using boost::mysql::error_code;
using boost::mysql::errc;

namespace
{

struct text_value_testcase : named_param
{
    std::string name;
    std::string from;
    value expected;
    protocol_field_type type;
    unsigned decimals;
    std::uint16_t flags;

    template <typename T>
    text_value_testcase(
        std::string name,
        std::string from,
        T&& expected_value,
        protocol_field_type type,
        std::uint16_t flags=0,
        unsigned decimals=0
    ) :
        name(std::move(name)),
        from(std::move(from)),
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
    text_value_testcase("geometry", "\1", "\1", protocol_field_type::geometry,
            column_flags::binary | column_flags::blob),

    // Anything we don't know what it is, we interpret as a string
    text_value_testcase("unknown_protocol_type", "test",
            "test", static_cast<protocol_field_type>(0x23))
), test_name_generator);

std::vector<text_value_testcase> make_int_cases(
    std::string signed_max_s,
    std::int64_t signed_max_b,
    std::string signed_min_s,
    std::int64_t signed_min_b,
    std::string unsigned_max_s,
    std::uint64_t unsigned_max_b,
    std::string zerofill_s,
    std::uint64_t zerofill_b,
    protocol_field_type type
)
{
    return {
        { "signed", "20", std::int64_t(20), type },
        { "signed_max", signed_max_s, signed_max_b, type },
        { "signed_negative", "-20", std::int64_t(-20), type },
        { "signed_min", signed_min_s, signed_min_b, type },
        { "unsigned", "20", std::uint64_t(20), type, column_flags::unsigned_ },
        { "usigned_min", "0", std::uint64_t(0), type, column_flags::unsigned_ },
        { "usigned_max", unsigned_max_s, unsigned_max_b, type, column_flags::unsigned_ },
        { "unsigned_zerofill", zerofill_s, std::uint64_t(zerofill_b), type,
                column_flags::unsigned_ | column_flags::zerofill }
    };
}

INSTANTIATE_TEST_SUITE_P(TINYINT, DeserializeTextValueTest, ValuesIn(
    make_int_cases(
        "127", 127,
        "-128", -128,
        "255", 255,
        "010", 10,
        protocol_field_type::tiny
    )
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(SMALLINT, DeserializeTextValueTest, ValuesIn(
    make_int_cases(
        "32767", 32767,
        "-32768", -32768,
        "65535", 65535,
        "00535", 535,
        protocol_field_type::short_
    )
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(MEDIUMINT, DeserializeTextValueTest, ValuesIn(
    make_int_cases(
        "8388607", 8388607,
        "-8388608",-8388608,
        "16777215", 16777215,
        "00007215", 7215,
        protocol_field_type::int24
    )
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(INT, DeserializeTextValueTest, ValuesIn(
    make_int_cases(
        "2147483647", 2147483647,
        "-2147483648", -2147483647 - 1, // minus is not part of literal, avoids warning
        "4294967295", 4294967295,
        "0000067295", 67295,
        protocol_field_type::long_
    )
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(BIGINT, DeserializeTextValueTest, ValuesIn(
    make_int_cases(
        "9223372036854775807", 9223372036854775807,
        "-9223372036854775808", -9223372036854775807 - 1, // minus is not part of literal, avoids warning
        "18446744073709551615", 18446744073709551615ULL, // suffix needed to avoid warning
        "0000067295", 67295,
        protocol_field_type::longlong
    )
), test_name_generator);

template <typename T>
std::vector<text_value_testcase> make_float_cases(
    protocol_field_type type
)
{
    return {
        { "zero", "0", T(0.0),  type },
        { "integer_positive", "4", T(4.0),  type },
        { "integer_negative", "-5", T(-5.0),  type },
        { "fractional_positive", "3.147", T(3.147),  type },
        { "fractional_negative", "-3.147", T(-3.147),  type },
        { "positive_exponent_positive_integer", "3e20", T(3e20),  type },
        { "positive_exponent_negative_integer", "-3e20", T(-3e20),  type },
        { "positive_exponent_positive_fractional", "3.14e20", T(3.14e20),  type },
        { "positive_exponent_negative_fractional", "-3.45e20", T(-3.45e20),  type },
        { "negative_exponent_positive_integer", "3e-20", T(3e-20),  type },
        { "negative_exponent_negative_integer", "-3e-20", T(-3e-20),  type },
        { "negative_exponent_positive_fractional", "3.14e-20", T(3.14e-20),  type },
        { "negative_exponent_negative_fractional", "-3.45e-20", T(-3.45e-20),  type }
    };
}

INSTANTIATE_TEST_SUITE_P(FLOAT, DeserializeTextValueTest, ValuesIn(
    make_float_cases<float>(protocol_field_type::float_)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(DOUBLE, DeserializeTextValueTest, ValuesIn(
    make_float_cases<double>(protocol_field_type::double_)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(DATE, DeserializeTextValueTest, Values(
    text_value_testcase("regular_date", "2019-02-28", makedate(2019, 2, 28), protocol_field_type::date),
    text_value_testcase("leap_year", "1788-02-29", makedate(1788, 2, 29), protocol_field_type::date),
    text_value_testcase("min", "0000-01-01", makedate(0, 1, 1), protocol_field_type::date),
    text_value_testcase("max", "9999-12-31", makedate(9999, 12, 31), protocol_field_type::date),
    text_value_testcase("zero", "0000-00-00", nullptr, protocol_field_type::date),
    text_value_testcase("zero_month", "0000-00-01", nullptr, protocol_field_type::date),
    text_value_testcase("zero_day", "0000-01-00", nullptr, protocol_field_type::date),
    text_value_testcase("zero_month_day_nonzero_year", "2010-00-00", nullptr, protocol_field_type::date),
    text_value_testcase("invalid_date", "2010-11-31", nullptr, protocol_field_type::date)
), test_name_generator);

std::vector<text_value_testcase> make_datetime_cases(
    protocol_field_type type
)
{
    std::vector<text_value_testcase> res {
        { "0_decimals_date", "2010-02-15 00:00:00", makedt(2010, 2, 15), type },
        { "0_decimals_h", "2010-02-15 02:00:00", makedt(2010, 2, 15, 2), type },
        { "0_decimals_hm", "2010-02-15 02:05:00", makedt(2010, 2, 15, 2, 5), type },
        { "0_decimals_hms", "2010-02-15 02:05:30", makedt(2010, 2, 15, 2, 5, 30), type },
        { "0_decimals_min", "0000-01-01 00:00:00", makedt(0, 1, 1), type },
        { "0_decimals_max", "9999-12-31 23:59:59", makedt(9999, 12, 31, 23, 59, 59), type },

        { "1_decimals_date", "2010-02-15 00:00:00.0", makedt(2010, 2, 15), type, 0, 1 },
        { "1_decimals_h", "2010-02-15 02:00:00.0", makedt(2010, 2, 15, 2), type, 0, 1 },
        { "1_decimals_hm", "2010-02-15 02:05:00.0", makedt(2010, 2, 15, 2, 5), type, 0, 1 },
        { "1_decimals_hms", "2010-02-15 02:05:30.0", makedt(2010, 2, 15, 2, 5, 30), type, 0, 1 },
        { "1_decimals_hmsu", "2010-02-15 02:05:30.5", makedt(2010, 2, 15, 2, 5, 30, 500000), type, 0, 1 },
        { "1_decimals_min", "0000-01-01 00:00:00.0", makedt(0, 1, 1), type, 0, 1 },
        { "1_decimals_max", "9999-12-31 23:59:59.9", makedt(9999, 12, 31, 23, 59, 59, 900000), type, 0, 1 },

        { "2_decimals_hms", "2010-02-15 02:05:30.00", makedt(2010, 2, 15, 2, 5, 30), type, 0, 2 },
        { "2_decimals_hmsu", "2010-02-15 02:05:30.05", makedt(2010, 2, 15, 2, 5, 30, 50000), type, 0, 2 },
        { "2_decimals_min", "0000-01-01 00:00:00.00", makedt(0, 1, 1), type, 0, 2 },
        { "2_decimals_max", "9999-12-31 23:59:59.99", makedt(9999, 12, 31, 23, 59, 59, 990000), type, 0, 2 },

        { "3_decimals_hms", "2010-02-15 02:05:30.000", makedt(2010, 2, 15, 2, 5, 30), type, 0, 3 },
        { "3_decimals_hmsu", "2010-02-15 02:05:30.420", makedt(2010, 2, 15, 2, 5, 30, 420000), type, 0, 3 },
        { "3_decimals_min", "0000-01-01 00:00:00.000", makedt(0, 1, 1), type, 0, 3 },
        { "3_decimals_max", "9999-12-31 23:59:59.999", makedt(9999, 12, 31, 23, 59, 59, 999000), type, 0, 3 },

        { "4_decimals_hms", "2010-02-15 02:05:30.0000", makedt(2010, 2, 15, 2, 5, 30), type, 0, 4 },
        { "4_decimals_hmsu", "2010-02-15 02:05:30.4267", makedt(2010, 2, 15, 2, 5, 30, 426700), type, 0, 4 },
        { "4_decimals_min", "0000-01-01 00:00:00.0000", makedt(0, 1, 1), type, 0, 4 },
        { "4_decimals_max", "9999-12-31 23:59:59.9999", makedt(9999, 12, 31, 23, 59, 59, 999900), type, 0, 4 },

        { "5_decimals_hms", "2010-02-15 02:05:30.00000", makedt(2010, 2, 15, 2, 5, 30), type, 0, 5 },
        { "5_decimals_hmsu", "2010-02-15 02:05:30.00239", makedt(2010, 2, 15, 2, 5, 30, 2390), type, 0, 5 },
        { "5_decimals_min", "0000-01-01 00:00:00.00000", makedt(0, 1, 1), type, 0, 5 },
        { "5_decimals_max", "9999-12-31 23:59:59.99999", makedt(9999, 12, 31, 23, 59, 59, 999990), type, 0, 5 },

        { "6_decimals_hms", "2010-02-15 02:05:30.000000", makedt(2010, 2, 15, 2, 5, 30), type, 0, 6 },
        { "6_decimals_hmsu", "2010-02-15 02:05:30.002395", makedt(2010, 2, 15, 2, 5, 30, 2395), type, 0, 6 },
        { "6_decimals_min", "0000-01-01 00:00:00.000000", makedt(0, 1, 1), type, 0, 6 },
        { "6_decimals_max", "9999-12-31 23:59:59.999999", makedt(9999, 12, 31, 23, 59, 59, 999999), type, 0, 6 },

        // not a real case, we cap decimals to 6
        { "7_decimals", "2010-02-15 02:05:30.002395", makedt(2010, 2, 15, 2, 5, 30, 2395), type, 0, 7 }
    };

    // Generate all invalid date casuistic for all decimals
    constexpr struct
    {
        const char* name;
        const char* base_value;
    } why_is_invalid [] = {
        { "zero", "0000-00-00 00:00:00" },
        { "invalid_date", "2010-11-31 01:10:59" },
        { "zero_month",  "2010-00-31 01:10:59" },
        { "zero_day", "2010-11-00 01:10:59" },
        { "zero_month_day", "2010-00-00 01:10:59" }
    };

    for (const auto& why: why_is_invalid)
    {
        for (unsigned decimals = 0; decimals <= 6; ++decimals)
        {
            std::string name = stringize(decimals, "_decimals_", why.name);
            std::string value = why.base_value;
            if (decimals > 0)
            {
                value.push_back('.');
                for (unsigned i = 0; i < decimals; ++i)
                    value.push_back('0');
            }
            res.emplace_back(std::move(name), std::move(value), nullptr, type, 0, decimals);
        }
    }

    return res;
}

INSTANTIATE_TEST_SUITE_P(DATETIME, DeserializeTextValueTest, ValuesIn(
    make_datetime_cases(protocol_field_type::datetime)
), test_name_generator);

// TIMESTAMPs use the same routines as DATETIMEs. They have a narrower range, so
// DATETIME range is tested instead. TIMESTAMPs have timezone, but we do not
// consider it right now.
INSTANTIATE_TEST_SUITE_P(TIMESTAMP, DeserializeTextValueTest, ValuesIn(
    make_datetime_cases(protocol_field_type::timestamp)
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
    text_value_testcase("0_decimals_negative_h0", "-00:51:23", -maket(0, 51, 23), protocol_field_type::time),

    text_value_testcase("1_decimals_positive_hms", "14:51:23.0", maket(14, 51, 23), protocol_field_type::time, 0, 1),
    text_value_testcase("1_decimals_positive_hmsu", "14:51:23.5", maket(14, 51, 23, 500000), protocol_field_type::time, 0, 1),
    text_value_testcase("1_decimals_max", "838:59:58.9", maket(838, 59, 58, 900000), protocol_field_type::time, 0, 1),
    text_value_testcase("1_decimals_negative_hms", "-14:51:23.0", -maket(14, 51, 23), protocol_field_type::time, 0, 1),
    text_value_testcase("1_decimals_negative_hmsu", "-14:51:23.5", -maket(14, 51, 23, 500000), protocol_field_type::time, 0, 1),
    text_value_testcase("1_decimals_min", "-838:59:58.9", -maket(838, 59, 58, 900000), protocol_field_type::time, 0, 1),
    text_value_testcase("1_decimals_zero", "00:00:00.0", maket(0, 0, 0), protocol_field_type::time, 0, 1),
    text_value_testcase("1_decimals_negative_h0", "-00:51:23.1", -maket(0, 51, 23, 100000), protocol_field_type::time, 0, 1),

    text_value_testcase("2_decimals_positive_hms", "14:51:23.00", maket(14, 51, 23), protocol_field_type::time, 0, 2),
    text_value_testcase("2_decimals_positive_hmsu", "14:51:23.52", maket(14, 51, 23, 520000), protocol_field_type::time, 0, 2),
    text_value_testcase("2_decimals_max", "838:59:58.99", maket(838, 59, 58, 990000), protocol_field_type::time, 0, 2),
    text_value_testcase("2_decimals_negative_hms", "-14:51:23.00", -maket(14, 51, 23), protocol_field_type::time, 0, 2),
    text_value_testcase("2_decimals_negative_hmsu", "-14:51:23.50", -maket(14, 51, 23, 500000), protocol_field_type::time, 0, 2),
    text_value_testcase("2_decimals_min", "-838:59:58.99", -maket(838, 59, 58, 990000), protocol_field_type::time, 0, 2),
    text_value_testcase("2_decimals_zero", "00:00:00.00", maket(0, 0, 0), protocol_field_type::time, 0, 2),
    text_value_testcase("2_decimals_negative_h0", "-00:51:23.12", -maket(0, 51, 23, 120000), protocol_field_type::time, 0, 2),

    text_value_testcase("3_decimals_positive_hms", "14:51:23.000", maket(14, 51, 23), protocol_field_type::time, 0, 3),
    text_value_testcase("3_decimals_positive_hmsu", "14:51:23.501", maket(14, 51, 23, 501000), protocol_field_type::time, 0, 3),
    text_value_testcase("3_decimals_max", "838:59:58.999", maket(838, 59, 58, 999000), protocol_field_type::time, 0, 3),
    text_value_testcase("3_decimals_negative_hms", "-14:51:23.000", -maket(14, 51, 23), protocol_field_type::time, 0, 3),
    text_value_testcase("3_decimals_negative_hmsu", "-14:51:23.003", -maket(14, 51, 23, 3000), protocol_field_type::time, 0, 3),
    text_value_testcase("3_decimals_min", "-838:59:58.999", -maket(838, 59, 58, 999000), protocol_field_type::time, 0, 3),
    text_value_testcase("3_decimals_zero", "00:00:00.000", maket(0, 0, 0), protocol_field_type::time, 0, 3),
    text_value_testcase("3_decimals_negative_h0", "-00:51:23.123", -maket(0, 51, 23, 123000), protocol_field_type::time, 0, 3),

    text_value_testcase("4_decimals_positive_hms", "14:51:23.0000", maket(14, 51, 23), protocol_field_type::time, 0, 4),
    text_value_testcase("4_decimals_positive_hmsu", "14:51:23.5017", maket(14, 51, 23, 501700), protocol_field_type::time, 0, 4),
    text_value_testcase("4_decimals_max", "838:59:58.9999", maket(838, 59, 58, 999900), protocol_field_type::time, 0, 4),
    text_value_testcase("4_decimals_negative_hms", "-14:51:23.0000", -maket(14, 51, 23), protocol_field_type::time, 0, 4),
    text_value_testcase("4_decimals_negative_hmsu", "-14:51:23.0038", -maket(14, 51, 23, 3800), protocol_field_type::time, 0, 4),
    text_value_testcase("4_decimals_min", "-838:59:58.9999", -maket(838, 59, 58, 999900), protocol_field_type::time, 0, 4),
    text_value_testcase("4_decimals_zero", "00:00:00.0000", maket(0, 0, 0), protocol_field_type::time, 0, 4),
    text_value_testcase("4_decimals_negative_h0", "-00:51:23.1234", -maket(0, 51, 23, 123400), protocol_field_type::time, 0, 4),

    text_value_testcase("5_decimals_positive_hms", "14:51:23.00000", maket(14, 51, 23), protocol_field_type::time, 0, 5),
    text_value_testcase("5_decimals_positive_hmsu", "14:51:23.50171", maket(14, 51, 23, 501710), protocol_field_type::time, 0, 5),
    text_value_testcase("5_decimals_max", "838:59:58.99999", maket(838, 59, 58, 999990), protocol_field_type::time, 0, 5),
    text_value_testcase("5_decimals_negative_hms", "-14:51:23.00000", -maket(14, 51, 23), protocol_field_type::time, 0, 5),
    text_value_testcase("5_decimals_negative_hmsu", "-14:51:23.00009", -maket(14, 51, 23, 90), protocol_field_type::time, 0, 5),
    text_value_testcase("5_decimals_min", "-838:59:58.99999", -maket(838, 59, 58, 999990), protocol_field_type::time, 0, 5),
    text_value_testcase("5_decimals_zero", "00:00:00.00000", maket(0, 0, 0), protocol_field_type::time, 0, 5),
    text_value_testcase("5_decimals_negative_h0", "-00:51:23.12345", -maket(0, 51, 23, 123450), protocol_field_type::time, 0, 5),

    text_value_testcase("6_decimals_positive_hms", "14:51:23.000000", maket(14, 51, 23), protocol_field_type::time, 0, 6),
    text_value_testcase("6_decimals_positive_hmsu", "14:51:23.501717", maket(14, 51, 23, 501717), protocol_field_type::time, 0, 6),
    text_value_testcase("6_decimals_max", "838:59:58.999999", maket(838, 59, 58, 999999), protocol_field_type::time, 0, 6),
    text_value_testcase("6_decimals_negative_hms", "-14:51:23.000000", -maket(14, 51, 23), protocol_field_type::time, 0, 6),
    text_value_testcase("6_decimals_negative_hmsu", "-14:51:23.900000", -maket(14, 51, 23, 900000), protocol_field_type::time, 0, 6),
    text_value_testcase("6_decimals_min", "-838:59:58.999999", -maket(838, 59, 58, 999999), protocol_field_type::time, 0, 6),
    text_value_testcase("6_decimals_zero", "00:00:00.000000", maket(0, 0, 0), protocol_field_type::time, 0, 6),
    text_value_testcase("6_decimals_negative_h0", "-00:51:23.123456", -maket(0, 51, 23, 123456), protocol_field_type::time, 0, 6),

    // This is not a real case - we cap anything above 6 decimals to 6
    text_value_testcase("7_decimals", "14:51:23.501717", maket(14, 51, 23, 501717), protocol_field_type::time, 0, 7)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(YEAR, DeserializeTextValueTest, Values(
    text_value_testcase("regular_value", "1999", std::uint64_t(1999), protocol_field_type::year, column_flags::unsigned_),
    text_value_testcase("min", "1901", std::uint64_t(1901), protocol_field_type::year, column_flags::unsigned_),
    text_value_testcase("max", "2155", std::uint64_t(2155), protocol_field_type::year, column_flags::unsigned_),
    text_value_testcase("zero", "0000", std::uint64_t(0), protocol_field_type::year, column_flags::unsigned_)
), test_name_generator);

} // anon namespace
