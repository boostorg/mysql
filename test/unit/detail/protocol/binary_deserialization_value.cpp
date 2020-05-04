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
// Note: full coverage of individual types is done in each type deserialization
// routine. Here we verify that deserialize_binary_value() selects the right
// deserialization routine and target value type. Full coverage for
// each type can be found in serialization.cpp

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
            protocol_field_type::longlong)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(FloatingPointTypes, DeserializeBinaryValueTest, Values(
    binary_value_testcase("float", {0x66, 0x66, 0x86, 0xc0}, -4.2f, protocol_field_type::float_),
    binary_value_testcase("double", {0xcd, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x10, 0xc0}, -4.2, protocol_field_type::double_)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(TimeTypes, DeserializeBinaryValueTest, Values(
    binary_value_testcase("date", {0x04, 0xda, 0x07, 0x03, 0x1c}, makedate(2010, 3, 28), protocol_field_type::date),
    binary_value_testcase("datetime", {0x0b, 0xda, 0x07, 0x05, 0x02, 0x17, 0x01, 0x32, 0xa0, 0x86, 0x01, 0x00},
            makedt(2010, 5, 2, 23, 1, 50, 100000), protocol_field_type::datetime),
    binary_value_testcase("timestamp", {0x0b, 0xda, 0x07, 0x05, 0x02, 0x17, 0x01, 0x32, 0xa0, 0x86, 0x01, 0x00},
            makedt(2010, 5, 2, 23, 1, 50, 100000), protocol_field_type::timestamp),
    binary_value_testcase("time", {  0x0c, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x02, 0x03, 0xa0, 0x86, 0x01, 0x00},
            maket(120, 2, 3, 100000), protocol_field_type::time),
    binary_value_testcase("year", {0xe3, 0x07}, std::uint32_t(2019), protocol_field_type::year, column_flags::unsigned_)
), test_name_generator);


} // anon namespace
