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
     err_binary_value_testcase("year_gt_max", {0x04, 0x10, 0x27, 0x03, 0x1c}, // year 10000
             protocol_field_type::date),
     err_binary_value_testcase("year_lt_min", {0x04, 0x63, 0x00, 0x03, 0x1c}, // year 99
             protocol_field_type::date)
));

INSTANTIATE_TEST_SUITE_P(YEAR, DeserializeBinaryValueErrorTest, ValuesIn(
    make_int_cases(protocol_field_type::year, 1)
), test_name_generator);

} // anon namespace
