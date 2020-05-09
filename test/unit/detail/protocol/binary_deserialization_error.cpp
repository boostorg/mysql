//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Test deserialize_binary_value(), only error cases

#include <gtest/gtest.h>
#include "boost/mysql/detail/protocol/binary_deserialization.hpp"
#include "test_common.hpp"

using namespace boost::mysql::detail;
using namespace boost::mysql::test;
using namespace testing;
using boost::mysql::value;
using boost::mysql::error_code;
using boost::mysql::errc;

namespace
{

struct err_binary_value_testcase : named_param
{
    std::string name;
    bytestring from;
    protocol_field_type type;
    std::uint16_t flags;
    errc expected_err;

    err_binary_value_testcase(std::string&& name, bytestring&& from, protocol_field_type type,
            std::uint16_t flags=0, errc expected_err=errc::protocol_value_error) :
        name(std::move(name)),
        from(std::move(from)),
        type(type),
        flags(flags),
        expected_err(expected_err)
    {
    }

    err_binary_value_testcase(std::string&& name, bytestring&& from, protocol_field_type type,
            errc expected_err) :
        name(std::move(name)),
        from(std::move(from)),
        type(type),
        flags(0),
        expected_err(expected_err)
    {
    }
};

struct DeserializeBinaryValueErrorTest : TestWithParam<err_binary_value_testcase>
{
};

TEST_P(DeserializeBinaryValueErrorTest, Error_ReturnsExpectedErrc)
{
    column_definition_packet coldef;
    coldef.type = GetParam().type;
    coldef.flags.value = GetParam().flags;
    boost::mysql::field_metadata meta (coldef);
    value actual_value;
    const auto& buff = GetParam().from;
    deserialization_context ctx (buff.data(), buff.data() + buff.size(), capabilities());
    auto err = deserialize_binary_value(ctx, meta, actual_value);
    auto expected = GetParam().expected_err;
    EXPECT_EQ(expected, err)
        << "expected: " << error_to_string(expected) << ", actual: " << error_to_string(err);
}

std::vector<err_binary_value_testcase> make_int_cases(
    protocol_field_type type,
    unsigned num_bytes
)
{
    return {
        { "signed_not_enough_space", bytestring(num_bytes, 0x0a),
            type, errc::incomplete_message },
        { "unsigned_not_enough_space", bytestring(num_bytes, 0x0a),
            type, column_flags::unsigned_, errc::incomplete_message }
    };
}


INSTANTIATE_TEST_SUITE_P(TINY, DeserializeBinaryValueErrorTest, ValuesIn(
    make_int_cases(protocol_field_type::tiny, 0)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(SMALLINT, DeserializeBinaryValueErrorTest, ValuesIn(
    make_int_cases(protocol_field_type::short_, 1)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(MEDIUMINT, DeserializeBinaryValueErrorTest, ValuesIn(
    make_int_cases(protocol_field_type::int24, 3)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(INT, DeserializeBinaryValueErrorTest, ValuesIn(
    make_int_cases(protocol_field_type::long_, 3)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(BIGINT, DeserializeBinaryValueErrorTest, ValuesIn(
    make_int_cases(protocol_field_type::longlong, 7)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(FLOAT, DeserializeBinaryValueErrorTest, Values(
    err_binary_value_testcase("not_enough_space", {0x01, 0x02, 0x03},
            protocol_field_type::float_, errc::incomplete_message),
    err_binary_value_testcase("inf", {0x00, 0x00, 0x80, 0x7f},
            protocol_field_type::float_),
    err_binary_value_testcase("minus_inf", {0x00, 0x00, 0x80, 0xff},
            protocol_field_type::float_),
    err_binary_value_testcase("nan", {0xff, 0xff, 0xff, 0x7f},
            protocol_field_type::float_),
    err_binary_value_testcase("minus_nan", {0xff, 0xff, 0xff, 0xff},
            protocol_field_type::float_)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(DOUBLE, DeserializeBinaryValueErrorTest, Values(
    err_binary_value_testcase("not_enough_space", {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07},
            protocol_field_type::double_, errc::incomplete_message),
    err_binary_value_testcase("inf", {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x7f},
            protocol_field_type::double_),
    err_binary_value_testcase("minus_inf", {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xff},
            protocol_field_type::double_),
    err_binary_value_testcase("nan", {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f},
            protocol_field_type::double_),
    err_binary_value_testcase("minus_nan", {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
            protocol_field_type::double_)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(DATE, DeserializeBinaryValueErrorTest, Values(
     err_binary_value_testcase("empty", {}, protocol_field_type::date, errc::incomplete_message),
     err_binary_value_testcase("incomplete_year", {0x04, 0xff},
             protocol_field_type::date, errc::incomplete_message),
     err_binary_value_testcase("no_month_day", {0x04, 0x09, 0x27},
             protocol_field_type::date, errc::incomplete_message),
     err_binary_value_testcase("no_day", {0x04, 0x09, 0x27, 0x01},
             protocol_field_type::date, errc::incomplete_message),
     err_binary_value_testcase("gt_max", {0x04, 0x10, 0x27, 0x0c, 0x1f}, // year 10000
             protocol_field_type::date),
     err_binary_value_testcase("protocol_max", {0x04, 0xff, 0xff, 0x0c, 0x1f},
             protocol_field_type::date)
));

std::vector<err_binary_value_testcase> make_datetime_cases(
    protocol_field_type type
)
{
    return {
        { "empty", {}, type, errc::incomplete_message },
        { "incomplete_date",    {0x04, 0x09, 0x27, 0x01}, type, errc::incomplete_message },
        { "no_hours_mins_secs", {0x07, 0x09, 0x27, 0x01, 0x01}, type, errc::incomplete_message },
        { "no_mins_secs",       {0x07, 0x09, 0x27, 0x01, 0x01, 0x01}, type, errc::incomplete_message },
        { "no_secs",            {0x07, 0x09, 0x27, 0x01, 0x01, 0x01, 0x01}, type, errc::incomplete_message },
        { "no_micros",          {0x0b, 0x09, 0x27, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00},
                type, errc::incomplete_message },
        { "date_gt_max",        {0x0b, 0xff, 0xff, 0x01, 0x01, 0x17, 0x01, 0x3b, 0x56, 0xc3, 0x0e, 0x00}, type },
        { "invalid_hour",       {0x0b, 0xda, 0x07, 0x01, 0x01,   24, 0x01, 0x3b, 0x56, 0xc3, 0x0e, 0x00}, type },
        { "invalid_hour_max",   {0x0b, 0xda, 0x07, 0x01, 0x01, 0xff, 0x01, 0x3b, 0x56, 0xc3, 0x0e, 0x00}, type },
        { "invalid_min",        {0x0b, 0xda, 0x07, 0x01, 0x01, 0x17,   60, 0x3b, 0x56, 0xc3, 0x0e, 0x00}, type },
        { "invalid_min_max",    {0x0b, 0xda, 0x07, 0x01, 0x01, 0x17, 0xff, 0x3b, 0x56, 0xc3, 0x0e, 0x00}, type },
        { "invalid_sec",        {0x0b, 0xda, 0x07, 0x01, 0x01, 0x17, 0x01,   60, 0x56, 0xc3, 0x0e, 0x00}, type },
        { "invalid_sec_max",    {0x0b, 0xda, 0x07, 0x01, 0x01, 0x17, 0x01, 0xff, 0x56, 0xc3, 0x0e, 0x00}, type },
        { "invalid_micro",      {0x0b, 0xda, 0x07, 0x01, 0x01, 0x17, 0x01, 0x3b, 0x40, 0x42, 0xf4, 0x00}, type }, // 1M
        { "invalid_micro_max",  {0x0b, 0xda, 0x07, 0x01, 0x01, 0x17, 0x01, 0x3b, 0xff, 0xff, 0xff, 0xff}, type },
        { "protocol_max",       {0xff, 0xff, 0xff,   12,   31, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, type }, // make date valid
    };
}

INSTANTIATE_TEST_SUITE_P(DATETIME, DeserializeBinaryValueErrorTest, ValuesIn(
    make_datetime_cases(protocol_field_type::datetime)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(TIMESTAMP, DeserializeBinaryValueErrorTest, ValuesIn(
    make_datetime_cases(protocol_field_type::timestamp)
), test_name_generator);

std::vector<err_binary_value_testcase> make_time_cases()
{
    constexpr auto type = protocol_field_type::time;
    std::vector<err_binary_value_testcase> res {
        { "empty",                        {},
                type, errc::incomplete_message },
        { "no_sign_days_hours_mins_secs", {0x08},
                type, errc::incomplete_message },
        { "no_days_hours_mins_secs",      {0x08, 0x01},
                type, errc::incomplete_message },
        { "no_hours_mins_secs",           {0x08, 0x01, 0x22, 0x00, 0x00, 0x00},
                type, errc::incomplete_message },
        { "no_mins_secs",                 {0x08, 0x01, 0x22, 0x00, 0x00, 0x00, 0x16},
                type, errc::incomplete_message },
        { "no_secs",                      {0x08, 0x01, 0x22, 0x00, 0x00, 0x00, 0x16, 0x3b},
                type, errc::incomplete_message },
        { "no_micros",                    {0x0c, 0x01, 0x22, 0x00, 0x00, 0x00, 0x16, 0x3b, 0x3a},
                type, errc::incomplete_message },
    };

    std::pair<const char*, std::vector<std::uint8_t>> out_of_range_cases [] {
        { "invalid_days",       {0x08, 0x00,   35, 0x00, 0x00, 0x00, 0x16, 0x3b, 0x3a} },
        { "invalid_days_max",   {0x08, 0x00, 0xff, 0xff, 0xff, 0xff, 0x16, 0x3b, 0x3a} },
        { "invalid_hours",      {0x08, 0x01, 0x22, 0x00, 0x00, 0x00,   24, 0x3b, 0x3a} },
        { "invalid_hours_max",  {0x08, 0x01, 0x22, 0x00, 0x00, 0x00, 0xff, 0x3b, 0x3a} },
        { "invalid_mins",       {0x08, 0x01, 0x22, 0x00, 0x00, 0x00, 0x16,   60, 0x3a} },
        { "invalid_mins_max",   {0x08, 0x01, 0x22, 0x00, 0x00, 0x00, 0x16, 0xff, 0x3a} },
        { "invalid_secs",       {0x08, 0x01, 0x22, 0x00, 0x00, 0x00, 0x16, 0x3b,   60} },
        { "invalid_secs_max",   {0x08, 0x01, 0x22, 0x00, 0x00, 0x00, 0x16, 0x3b, 0xff} },
        { "invalid_micros",     {0x0c, 0x01, 0x22, 0x00, 0x00, 0x00, 0x16, 0x3b, 0x3a, 0x40, 0x42, 0xf4, 0x00} },
        { "invalid_micros_max", {0x0c, 0x01, 0x22, 0x00, 0x00, 0x00, 0x16, 0x3b, 0x3a, 0xff, 0xff, 0xff, 0xff} },
    };

    for (auto& c: out_of_range_cases)
    {
        // Positive
        c.second[1] = 0x00;
        res.emplace_back(c.first + std::string("_positive"), bytestring(c.second), type);

        // Negative
        c.second[1] = 0x01;
        res.emplace_back(c.first + std::string("_negative"), std::move(c.second), type);
    }

    return res;
}

// 0x08, 0x01, 0x22, 0x00, 0x00, 0x00, 0x16, 0x3b, 0x3a
// 0x0c, 0x01, 0x22, 0x00, 0x00, 0x00, 0x16, 0x3b, 0x3a, 0x58, 0x3e, 0x0f, 0x00
INSTANTIATE_TEST_SUITE_P(TIME, DeserializeBinaryValueErrorTest, ValuesIn(
    make_time_cases()
), test_name_generator);


INSTANTIATE_TEST_SUITE_P(YEAR, DeserializeBinaryValueErrorTest, ValuesIn(
    make_int_cases(protocol_field_type::year, 1)
), test_name_generator);

} // anon namespace
