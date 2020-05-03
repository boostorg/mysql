//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Test deserialize_text_value(), only error cases

#include <gtest/gtest.h>
#include "boost/mysql/detail/protocol/text_deserialization.hpp"
#include "test_common.hpp"

using namespace boost::mysql::detail;
using namespace boost::mysql::test;
using namespace testing;
using boost::mysql::value;
using boost::mysql::error_code;
using boost::mysql::errc;

namespace
{

struct err_text_value_testcase : named_param
{
    std::string name;
    std::string_view from;
    protocol_field_type type;
    std::uint16_t flags;
    unsigned decimals;
    errc expected_err;

    err_text_value_testcase(std::string&& name, std::string_view from, protocol_field_type type,
            std::uint16_t flags=0, unsigned decimals=0, errc expected_err=errc::protocol_value_error) :
        name(std::move(name)),
        from(from),
        type(type),
        flags(flags),
        decimals(decimals),
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
    return make_int_err_cases(t, "-2147483649", "2147483648", "-4294967296", "4294967296");
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
        "-18446744073709551616",
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

INSTANTIATE_TEST_SUITE_P(DATE, DeserializeTextValueErrorTest, Values(
    err_text_value_testcase("empty",            "", protocol_field_type::date),
    err_text_value_testcase("too_short",        "2020-05-2", protocol_field_type::date),
    err_text_value_testcase("too_long",         "02020-05-02", protocol_field_type::date),
    err_text_value_testcase("invalid_date",     "2020-04-31", protocol_field_type::date),
    err_text_value_testcase("bad_delimiter",    "2020:05:02", protocol_field_type::date),
    err_text_value_testcase("too_many_groups",  "20-20-05-2", protocol_field_type::date),
    err_text_value_testcase("too_few_groups",   "2020-00005", protocol_field_type::date),
    err_text_value_testcase("incomplete_year",  "999-05-005", protocol_field_type::date),
    err_text_value_testcase("null_value",       makesv("2020-05-\02"), protocol_field_type::date),
    err_text_value_testcase("lt_min",           "0099-05-02", protocol_field_type::date),
    err_text_value_testcase("gt_max",           "10000-05-2", protocol_field_type::date),
    err_text_value_testcase("long_month",       "2010-005-2", protocol_field_type::date),
    err_text_value_testcase("long_day",         "2010-5-002", protocol_field_type::date),
    err_text_value_testcase("negative_year",    "-999-05-02", protocol_field_type::date),
    err_text_value_testcase("negative_month",   "2010--5-02", protocol_field_type::date),
    err_text_value_testcase("negative_day",     "2010-05--                                                                                2", protocol_field_type::date)
), test_name_generator);

std::vector<err_text_value_testcase> make_datetime_err_cases(
    protocol_field_type t
)
{
    return {
        err_text_value_testcase("empty",           "", t),
        err_text_value_testcase("too_short_0",     "2020-05-02 23:01:0", t, 0, 0),
        err_text_value_testcase("too_short_1",     "2020-05-02 23:01:0.1", t, 0, 1),
        err_text_value_testcase("too_short_2",     "2020-05-02 23:01:00.1", t, 0, 2),
        err_text_value_testcase("too_short_3",     "2020-05-02 23:01:00.11", t, 0, 3),
        err_text_value_testcase("too_short_4",     "2020-05-02 23:01:00.111", t, 0, 4),
        err_text_value_testcase("too_short_5",     "2020-05-02 23:01:00.1111", t, 0, 5),
        err_text_value_testcase("too_short_6",     "2020-05-02 23:01:00.11111", t, 0, 6),
        err_text_value_testcase("too_long_0",      "2020-05-02 23:01:00.8", t, 0, 0),
        err_text_value_testcase("too_long_1",      "2020-05-02 23:01:00.98", t, 0, 1),
        err_text_value_testcase("too_long_2",      "2020-05-02 23:01:00.998", t, 0, 2),
        err_text_value_testcase("too_long_3",      "2020-05-02 23:01:00.9998", t, 0, 3),
        err_text_value_testcase("too_long_4",      "2020-05-02 23:01:00.99998", t, 0, 4),
        err_text_value_testcase("too_long_5",      "2020-05-02 23:01:00.999998", t, 0, 5),
        err_text_value_testcase("too_long_6",      "2020-05-02 23:01:00.9999998", t, 0, 6),
        err_text_value_testcase("no_decimals_1",   "2020-05-02 23:01:00  ", t, 0, 1),
        err_text_value_testcase("no_decimals_2",   "2020-05-02 23:01:00   ", t, 0, 2),
        err_text_value_testcase("no_decimals_3",   "2020-05-02 23:01:00     ", t, 0, 3),
        err_text_value_testcase("no_decimals_4",   "2020-05-02 23:01:00      ", t, 0, 4),
        err_text_value_testcase("no_decimals_5",   "2020-05-02 23:01:00       ", t, 0, 5),
        err_text_value_testcase("no_decimals_6",   "2020-05-02 23:01:00        ", t, 0, 6),
        err_text_value_testcase("trailing_0",      "2020-05-02 23:01:0p", t, 0, 0),
        err_text_value_testcase("trailing_1",      "2020-05-02 23:01:00.p", t, 0, 1),
        err_text_value_testcase("trailing_2",      "2020-05-02 23:01:00.1p", t, 0, 2),
        err_text_value_testcase("trailing_3",      "2020-05-02 23:01:00.12p", t, 0, 3),
        err_text_value_testcase("trailing_4",      "2020-05-02 23:01:00.123p", t, 0, 4),
        err_text_value_testcase("trailing_5",      "2020-05-02 23:01:00.1234p", t, 0, 5),
        err_text_value_testcase("trailing_6",      "2020-05-02 23:01:00.12345p", t, 0, 6),
        err_text_value_testcase("bad_delimiter",   "2020-05-02 23-01-00", t),
        err_text_value_testcase("missing_1gp_0",   "2020-05-02 23:01:  ", t),
        err_text_value_testcase("missing_2gp_0",   "2020-05-02 23:     ", t),
        err_text_value_testcase("missing_3gp_0",   "2020-05-02         ", t),
        err_text_value_testcase("missing_1gp_1",   "2020-05-02 23:01:.9  ", t),
        err_text_value_testcase("missing_2gp_1",   "2020-05-02 23:.9     ", t),
        err_text_value_testcase("missing_3gp_1",   "2020-05-02.9         ", t),
        err_text_value_testcase("invalid_hour1",   "2020-05-02 24:20:20", t),
        err_text_value_testcase("invalid_hour2",   "2020-05-02 240:2:20", t),
        err_text_value_testcase("negative_hour",   "2020-05-02 -2:20:20", t),
        err_text_value_testcase("invalid_min",     "2020-05-02 22:60:20", t),
        err_text_value_testcase("negative_min",    "2020-05-02 22:-1:20", t),
        err_text_value_testcase("invalid_sec",     "2020-05-02 22:06:60", t),
        err_text_value_testcase("negative_sec",    "2020-05-02 22:06:-1", t),
        err_text_value_testcase("negative_micro",  "2020-05-02 22:06:01.-1", t, 0, 2),
        err_text_value_testcase("lt_min",          "0090-01-01 00:00:00.00", t, 0, 2),
        err_text_value_testcase("gt_max",          "10000-01-01 00:00:00.00", t, 0, 2)
    };
}

INSTANTIATE_TEST_SUITE_P(DATETIME, DeserializeTextValueErrorTest, ValuesIn(
    make_datetime_err_cases(protocol_field_type::datetime)
), test_name_generator);

// TIMESTAMP has a more narrow range than DATETIME, but as both are represented the same,
// we accept the same, wider range for both.
INSTANTIATE_TEST_SUITE_P(TIMESTAMP, DeserializeTextValueErrorTest, ValuesIn(
    make_datetime_err_cases(protocol_field_type::timestamp)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(TIME, DeserializeTextValueErrorTest, Values(
    err_text_value_testcase("empty",           "", protocol_field_type::time),
    err_text_value_testcase("not_numbers",     "abjkjdb67", protocol_field_type::time),
    err_text_value_testcase("too_short_0",     "1:20:20", protocol_field_type::time),
    err_text_value_testcase("too_short_1",     "1:20:20.1", protocol_field_type::time, 0, 1),
    err_text_value_testcase("too_short_2",     "01:20:20.1", protocol_field_type::time, 0, 2),
    err_text_value_testcase("too_short_3",     "01:20:20.12", protocol_field_type::time, 0, 3),
    err_text_value_testcase("too_short_4",     "01:20:20.123", protocol_field_type::time, 0, 4),
    err_text_value_testcase("too_short_5",     "01:20:20.1234", protocol_field_type::time, 0, 5),
    err_text_value_testcase("too_short_6",     "01:20:20.12345", protocol_field_type::time, 0, 6),
    err_text_value_testcase("too_long_0",      "-9999:40:40", protocol_field_type::time, 0, 0),
    err_text_value_testcase("too_long_1",      "-9999:40:40.1", protocol_field_type::time, 0, 1),
    err_text_value_testcase("too_long_2",      "-9999:40:40.12", protocol_field_type::time, 0, 2),
    err_text_value_testcase("too_long_3",      "-9999:40:40.123", protocol_field_type::time, 0, 3),
    err_text_value_testcase("too_long_4",      "-9999:40:40.1234", protocol_field_type::time, 0, 4),
    err_text_value_testcase("too_long_5",      "-9999:40:40.12345", protocol_field_type::time, 0, 5),
    err_text_value_testcase("too_long_6",      "-9999:40:40.123456", protocol_field_type::time, 0, 6),
    err_text_value_testcase("extra_long",      "-99999999:40:40.12345678", protocol_field_type::time, 0, 6),
    err_text_value_testcase("extra_long2",     "99999999999:40:40", protocol_field_type::time, 0, 6),
    err_text_value_testcase("decimals_0",      "01:20:20.1", protocol_field_type::time, 0, 0),
    err_text_value_testcase("no_decimals_1",   "01:20:20  ", protocol_field_type::time, 0, 1),
    err_text_value_testcase("no_decimals_2",   "01:20:20   ", protocol_field_type::time, 0, 2),
    err_text_value_testcase("no_decimals_3",   "01:20:20    ", protocol_field_type::time, 0, 3),
    err_text_value_testcase("no_decimals_4",   "01:20:20     ", protocol_field_type::time, 0, 4),
    err_text_value_testcase("no_decimals_5",   "01:20:20      ", protocol_field_type::time, 0, 5),
    err_text_value_testcase("no_decimals_6",   "01:20:20       ", protocol_field_type::time, 0, 6),
    err_text_value_testcase("bad_delimiter",   "01-20-20", protocol_field_type::time),
    err_text_value_testcase("missing_1gp_0",   "23:01:  ", protocol_field_type::time),
    err_text_value_testcase("missing_2gp_0",   "23:     ", protocol_field_type::time),
    err_text_value_testcase("missing_1gp_1",   "23:01:.9  ", protocol_field_type::time, 0, 1),
    err_text_value_testcase("missing_2gp_1",   "23:.9     ", protocol_field_type::time, 0, 1),
    err_text_value_testcase("invalid_min",     "22:60:20", protocol_field_type::time),
    err_text_value_testcase("negative_min",    "22:-1:20", protocol_field_type::time),
    err_text_value_testcase("invalid_sec",     "22:06:60", protocol_field_type::time),
    err_text_value_testcase("negative_sec",    "22:06:-1", protocol_field_type::time),
    err_text_value_testcase("invalid_micro_1", "22:06:01.99", protocol_field_type::time, 0, 1),
    err_text_value_testcase("invalid_micro_2", "22:06:01.999", protocol_field_type::time, 0, 2),
    err_text_value_testcase("invalid_micro_3", "22:06:01.9999", protocol_field_type::time, 0, 3),
    err_text_value_testcase("invalid_micro_4", "22:06:01.99999", protocol_field_type::time, 0, 4),
    err_text_value_testcase("invalid_micro_5", "22:06:01.999999", protocol_field_type::time, 0, 5),
    err_text_value_testcase("invalid_micro_6", "22:06:01.9999999", protocol_field_type::time, 0, 6),
    err_text_value_testcase("negative_micro",  "22:06:01.-1", protocol_field_type::time, 0, 2),
    err_text_value_testcase("lt_min",          "-900:00:00.00", protocol_field_type::time, 0, 2),
    err_text_value_testcase("gt_max",          "900:00:00.00", protocol_field_type::time, 0, 2),
    err_text_value_testcase("invalid_sign",    "x670:00:00.00", protocol_field_type::time, 0, 2),
    err_text_value_testcase("null_char",       makesv("20:00:\00.00"), protocol_field_type::time, 0, 2),
    err_text_value_testcase("trailing_0",      "22:06:01k", protocol_field_type::time, 0, 0),
    err_text_value_testcase("trailing_1",      "22:06:01.1k", protocol_field_type::time, 0, 1),
    err_text_value_testcase("trailing_2",      "22:06:01.12k", protocol_field_type::time, 0, 2),
    err_text_value_testcase("trailing_3",      "22:06:01.123k", protocol_field_type::time, 0, 3),
    err_text_value_testcase("trailing_4",      "22:06:01.1234k", protocol_field_type::time, 0, 4),
    err_text_value_testcase("trailing_5",      "22:06:01.12345k", protocol_field_type::time, 0, 5),
    err_text_value_testcase("trailing_6",      "22:06:01.123456k", protocol_field_type::time, 0, 6),
    err_text_value_testcase("double_sign",     "--22:06:01.123456", protocol_field_type::time, 0, 6)
), test_name_generator);

// Although range is smaller, YEAR is deserialized just as a regular int
INSTANTIATE_TEST_SUITE_P(YEAR, DeserializeTextValueErrorTest, ValuesIn(
    make_int32_err_cases(protocol_field_type::year)
), test_name_generator);

} // anon namespace
