//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <gtest/gtest.h>
#include "boost/mysql/detail/protocol/binary_deserialization.hpp"
#include "test_common.hpp"

// Tests for deserialize_binary_value()

using namespace boost::mysql::detail;
using namespace boost::mysql::test;
using namespace testing;
using boost::mysql::value;
using boost::mysql::error_code;
using boost::mysql::errc;

namespace
{

using boost::mysql::operator<<;


struct binary_value_testcase : named_param
{
    std::string name;
    std::vector<std::uint8_t> from;
    value expected;
    protocol_field_type type;
    std::uint16_t flags;

    template <typename T>
    binary_value_testcase(
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

struct DeserializeBinaryValueTest : public TestWithParam<binary_value_testcase> {};

TEST_P(DeserializeBinaryValueTest, CorrectFormat_SetsOutputValueReturnsTrue)
{
    using boost::mysql::operator<<;
    column_definition_packet coldef;
    coldef.type = GetParam().type;
    coldef.flags.value = GetParam().flags;
    boost::mysql::field_metadata meta (coldef);
    value actual_value;
    const auto& buffer = GetParam().from;
    deserialization_context ctx (buffer.data(), buffer.data() + buffer.size(), capabilities());
    auto err = deserialize_binary_value(ctx, meta, actual_value);
    EXPECT_EQ(err, errc::ok);
    EXPECT_EQ(actual_value, GetParam().expected);
    EXPECT_EQ(ctx.first(), buffer.data() + buffer.size()); // all bytes consumed
}

INSTANTIATE_TEST_SUITE_P(StringTypes, DeserializeBinaryValueTest, Values(
    binary_value_testcase("varchar", {0x04, 0x74, 0x65, 0x73, 0x74}, "test", protocol_field_type::var_string),
    binary_value_testcase("char", {0x04, 0x74, 0x65, 0x73, 0x74}, "test", protocol_field_type::string),
    binary_value_testcase("varbinary", {0x04, 0x74, 0x65, 0x73, 0x74}, "test",
            protocol_field_type::var_string, column_flags::binary),
    binary_value_testcase("binary", {0x04, 0x74, 0x65, 0x73, 0x74}, "test",
            protocol_field_type::string, column_flags::binary),
    binary_value_testcase("text_blob", {0x04, 0x74, 0x65, 0x73, 0x74}, "test",
            protocol_field_type::blob, column_flags::blob),
    binary_value_testcase("enum", {0x04, 0x74, 0x65, 0x73, 0x74}, "test",
            protocol_field_type::string, column_flags::enum_),
    binary_value_testcase("set", {0x04, 0x74, 0x65, 0x73, 0x74}, "test",
            protocol_field_type::string, column_flags::set),

    binary_value_testcase("bit", {0x02, 0x02, 0x01}, "\2\1", protocol_field_type::bit),
    binary_value_testcase("decimal", {0x02, 0x31, 0x30}, "10", protocol_field_type::newdecimal),
    binary_value_testcase("geomtry", {0x04, 0x74, 0x65, 0x73, 0x74}, "test", protocol_field_type::geometry)
), test_name_generator);

// Note: these employ regular integer deserialization functions, which have
// already been tested in serialization.cpp
INSTANTIATE_TEST_SUITE_P(IntTypes, DeserializeBinaryValueTest, Values(
    binary_value_testcase("tinyint_unsigned", {0x14}, std::uint32_t(20),
            protocol_field_type::tiny, column_flags::unsigned_),
    binary_value_testcase("tinyint_signed", {0xec}, std::int32_t(-20), protocol_field_type::tiny),

    binary_value_testcase("smallint_unsigned", {0x14, 0x00}, std::uint32_t(20),
            protocol_field_type::short_, column_flags::unsigned_),
    binary_value_testcase("smallint_signed", {0xec, 0xff}, std::int32_t(-20), protocol_field_type::short_),

    binary_value_testcase("mediumint_unsigned", {0x14, 0x00, 0x00, 0x00}, std::uint32_t(20),
            protocol_field_type::int24, column_flags::unsigned_),
    binary_value_testcase("mediumint_signed", {0xec, 0xff, 0xff, 0xff}, std::int32_t(-20), protocol_field_type::int24),

    binary_value_testcase("int_unsigned", {0x14, 0x00, 0x00, 0x00}, std::uint32_t(20),
            protocol_field_type::long_, column_flags::unsigned_),
    binary_value_testcase("int_signed", {0xec, 0xff, 0xff, 0xff}, std::int32_t(-20), protocol_field_type::long_),

    binary_value_testcase("bigint_unsigned", {0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, std::uint64_t(20),
            protocol_field_type::longlong, column_flags::unsigned_),
    binary_value_testcase("bigint_signed", {0xec, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, std::int64_t(-20),
            protocol_field_type::longlong),
    binary_value_testcase("year", {0xe3, 0x07}, std::uint32_t(2019),
            protocol_field_type::year, column_flags::unsigned_)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(FLOAT, DeserializeBinaryValueTest, Values(
    binary_value_testcase("fractional_negative", {0x66, 0x66, 0x86, 0xc0},
            -4.2f, protocol_field_type::float_),
    binary_value_testcase("fractional_positive", {0x66, 0x66, 0x86, 0x40},
            4.2f, protocol_field_type::float_),
    binary_value_testcase("positive_exp_positive_fractional", {0x01, 0x2d, 0x88, 0x61},
            3.14e20f, protocol_field_type::float_),
    binary_value_testcase("zero", {0x00, 0x00, 0x00, 0x00},
            0.0f, protocol_field_type::float_)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(DOUBLE, DeserializeBinaryValueTest, Values(
    binary_value_testcase("fractional_negative", {0xcd, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x10, 0xc0},
            -4.2, protocol_field_type::double_),
    binary_value_testcase("fractional_positive", {0xcd, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x10, 0x40},
            4.2, protocol_field_type::double_),
    binary_value_testcase("positive_exp_positive_fractional", {0xce, 0x46, 0x3c, 0x76, 0x9c, 0x68, 0x90, 0x69},
            3.14e200, protocol_field_type::double_),
    binary_value_testcase("zero", {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
            0.0, protocol_field_type::double_)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(DATE, DeserializeBinaryValueTest, ::testing::Values(
    binary_value_testcase("regular", {0x04, 0xda, 0x07, 0x03, 0x1c},
            makedate(2010, 3, 28), protocol_field_type::date),
    binary_value_testcase("min", {0x04, 0xe8, 0x03, 0x01, 0x01},
            makedate(1000, 1, 1), protocol_field_type::date),
    binary_value_testcase("max", {0x04, 0x0f, 0x27, 0x0c, 0x1f},
            makedate(9999, 12, 31), protocol_field_type::date)
), test_name_generator);

std::vector<binary_value_testcase> make_datetime_cases(
    protocol_field_type type
)
{
    return {
        { "only_date", {0x04, 0xda, 0x07, 0x01, 0x01},
            makedt(2010, 1, 1), type },
        { "date_h", {0x07, 0xda, 0x07, 0x01, 0x01, 0x14, 0x00, 0x00},
                makedt(2010, 1, 1, 20,  0,  0,  0), type },
        { "date_m", {0x07, 0xda, 0x07, 0x01, 0x01, 0x00, 0x01, 0x00},
                makedt(2010, 1, 1, 0,   1,  0,  0), type },
        { "date_hm", {0x07, 0xda, 0x07, 0x01, 0x01, 0x03, 0x02, 0x00},
                makedt(2010, 1, 1, 3,   2,  0,  0), type },
        { "date_s", {0x07, 0xda, 0x07, 0x01, 0x01, 0x00, 0x00, 0x01},
                makedt(2010, 1, 1, 0,   0,  1,  0), type },
        { "date_ms", {0x07, 0xda, 0x07, 0x01, 0x01, 0x00, 0x3b, 0x01},
                makedt(2010, 1, 1, 0,   59, 1,  0), type },
        { "date_hs", {0x07, 0xda, 0x07, 0x01, 0x01, 0x05, 0x00, 0x01},
                makedt(2010, 1, 1, 5,   0,  1,  0), type },
        { "date_hms", {0x07, 0xda, 0x07, 0x01, 0x01, 0x17, 0x01, 0x3b},
                makedt(2010, 1, 1, 23,  1,  59, 0), type },
        { "date_u", {0x0b, 0xda, 0x07, 0x01, 0x01, 0x00, 0x00, 0x00, 0x78, 0xd4, 0x03, 0x00},
                makedt(2010, 1, 1, 0,   0,  0,  251000), type },
        { "date_hu", {0x0b, 0xda, 0x07, 0x01, 0x01, 0x17, 0x00, 0x00, 0x56, 0xc3, 0x0e, 0x00},
                makedt(2010, 1, 1, 23,  0,  0,  967510), type },
        { "date_mu", {0x0b, 0xda, 0x07, 0x01, 0x01, 0x00, 0x01, 0x00, 0x56, 0xc3, 0x0e, 0x00},
                makedt(2010, 1, 1, 0,   1,  0,  967510), type },
        { "date_hmu", {0x0b, 0xda, 0x07, 0x01, 0x01, 0x17, 0x01, 0x00, 0x56, 0xc3, 0x0e, 0x00},
                makedt(2010, 1, 1, 23,  1,  0,  967510), type },
        { "date_su", {0x0b, 0xda, 0x07, 0x01, 0x01, 0x00, 0x00, 0x3b, 0x56, 0xc3, 0x0e, 0x00},
                makedt(2010, 1, 1, 0,   0,  59, 967510), type },
        { "date_msu", {0x0b, 0xda, 0x07, 0x01, 0x01, 0x00, 0x01, 0x3b, 0x56, 0xc3, 0x0e, 0x00},
                makedt(2010, 1, 1, 0,   1,  59, 967510), type },
        { "date_hsu", {0x0b, 0xda, 0x07, 0x01, 0x01, 0x17, 0x00, 0x3b, 0x56, 0xc3, 0x0e, 0x00},
                makedt(2010, 1, 1, 23,  0,  59, 967510), type },
        { "date_hmsu", {0x0b, 0xda, 0x07, 0x01, 0x01, 0x17, 0x01, 0x3b, 0x56, 0xc3, 0x0e, 0x00},
                makedt(2010, 1, 1, 23,  1,  59, 967510), type },
    };
}

INSTANTIATE_TEST_SUITE_P(DATETIME, DeserializeBinaryValueTest, ValuesIn(
    make_datetime_cases(protocol_field_type::datetime)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(TIMESTAMP, DeserializeBinaryValueTest, ValuesIn(
    make_datetime_cases(protocol_field_type::timestamp)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(TIME, DeserializeBinaryValueTest, Values(
    binary_value_testcase("zero", {0x00},
            maket(0, 0, 0), protocol_field_type::time),
    binary_value_testcase("positive_d", {0x08, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
            maket(48,  0,  0, 0), protocol_field_type::time),
    binary_value_testcase("positive_h", {0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00},
            maket(21, 0, 0, 0), protocol_field_type::time),
    binary_value_testcase("positive_m", {0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x00},
            maket(0, 40, 0), protocol_field_type::time),
    binary_value_testcase("positive_s", {0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x15},
            maket(0, 0, 21), protocol_field_type::time),
    binary_value_testcase("positive_u", {0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe8, 0xe5, 0x04, 0x00},
            maket(0, 0, 0, 321000), protocol_field_type::time),
    binary_value_testcase("positive_hmsu", {0x0c, 0x00, 0x22, 0x00, 0x00, 0x00, 0x16, 0x3b, 0x3a, 0x58, 0x3e, 0x0f, 0x00},
            maket(838, 59, 58, 999000), protocol_field_type::time),
    binary_value_testcase("negative_d", {0x08, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
            -maket(48,  0,  0, 0), protocol_field_type::time),
    binary_value_testcase("negative_h", {0x08, 0x01, 0x00, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00},
            -maket(21, 0, 0, 0), protocol_field_type::time),
    binary_value_testcase("negative_m", {0x08, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x00},
            -maket(0, 40, 0), protocol_field_type::time),
    binary_value_testcase("negative_s", {0x08, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x15},
            -maket(0, 0, 21), protocol_field_type::time),
    binary_value_testcase("negative_u", {0x0c, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe8, 0xe5, 0x04, 0x00},
            -maket(0, 0, 0, 321000), protocol_field_type::time),
    binary_value_testcase("negative_hmsu", {0x0c, 0x01, 0x22, 0x00, 0x00, 0x00, 0x16, 0x3b, 0x3a, 0x58, 0x3e, 0x0f, 0x00},
            -maket(838, 59, 58, 999000), protocol_field_type::time)
), test_name_generator);


} // anon namespace
