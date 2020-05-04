//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Test deserialize_binary_row()

#include <gtest/gtest.h>
#include "boost/mysql/detail/protocol/binary_deserialization.hpp"
#include "test_common.hpp"

using namespace boost::mysql::detail;
using namespace boost::mysql::test;
using namespace testing;
using boost::mysql::value;
using boost::mysql::collation;
using boost::mysql::error_code;
using boost::mysql::errc;

namespace
{

using boost::mysql::operator<<;

std::vector<boost::mysql::field_metadata> make_meta(
    const std::vector<protocol_field_type>& types
)
{
    std::vector<boost::mysql::field_metadata> res;
    for (const auto type: types)
    {
        column_definition_packet coldef;
        coldef.type = type;
        res.emplace_back(coldef);
    }
    return res;
}


struct binary_row_testcase : named_param
{
    std::string name;
    std::vector<std::uint8_t> from;
    std::vector<value> expected;
    std::vector<protocol_field_type> types;

    binary_row_testcase(
        std::string name,
        std::vector<std::uint8_t> from,
        std::vector<value> expected,
        std::vector<protocol_field_type> types
    ):
        name(std::move(name)),
        from(std::move(from)),
        expected(std::move(expected)),
        types(std::move(types))
    {
        assert(expected.size() == types.size());
    }
};

struct DeserializeBinaryRowTest : public TestWithParam<binary_row_testcase> {};

TEST_P(DeserializeBinaryRowTest, CorrectFormat_SetsOutputValueReturnsTrue)
{
    auto meta = make_meta(GetParam().types);
    const auto& buffer = GetParam().from;
    deserialization_context ctx (buffer.data(), buffer.data() + buffer.size(), capabilities());

    std::vector<value> actual;
    auto err = deserialize_binary_row(ctx, meta, actual);
    EXPECT_EQ(err, error_code());
    EXPECT_EQ(actual, GetParam().expected);
}

INSTANTIATE_TEST_SUITE_P(Default, DeserializeBinaryRowTest, testing::Values(
    binary_row_testcase("one_value", {0x00, 0x00, 0x14}, makevalues(std::int32_t(20)), {protocol_field_type::tiny}),
    binary_row_testcase("one_null", {0x00, 0x04}, makevalues(nullptr), {protocol_field_type::tiny}),
    binary_row_testcase("two_values", {0x00, 0x00, 0x03, 0x6d, 0x69, 0x6e, 0x6d, 0x07},
            makevalues("min", std::int32_t(1901)), {protocol_field_type::var_string, protocol_field_type::short_}),
    binary_row_testcase("one_value_one_null", {0x00, 0x08, 0x03, 0x6d, 0x61, 0x78},
            makevalues("max", nullptr), {protocol_field_type::var_string, protocol_field_type::tiny}),
    binary_row_testcase("two_nulls", {0x00, 0x0c},
            makevalues(nullptr, nullptr), {protocol_field_type::tiny, protocol_field_type::tiny}),
    binary_row_testcase("six_nulls", {0x00, 0xfc}, std::vector<value>(6, value(nullptr)),
            std::vector<protocol_field_type>(6, protocol_field_type::tiny)),
    binary_row_testcase("seven_nulls", {0x00, 0xfc, 0x01}, std::vector<value>(7, value(nullptr)),
            std::vector<protocol_field_type>(7, protocol_field_type::tiny)),
    binary_row_testcase("several_values", {
            0x00, 0x90, 0x00, 0xfd, 0x14, 0x00, 0xc3, 0xf5, 0x48,
            0x40, 0x02, 0x61, 0x62, 0x04, 0xe2, 0x07, 0x0a,
            0x05, 0x71, 0x99, 0x6d, 0xe2, 0x93, 0x4d, 0xf5,
            0x3d
        }, makevalues(
            std::int32_t(-3),
            std::int32_t(20),
            nullptr,
            3.14f,
            "ab",
            nullptr,
            makedate(2018, 10, 5),
            3.10e-10
        ), {
            protocol_field_type::tiny,
            protocol_field_type::short_,
            protocol_field_type::long_,
            protocol_field_type::float_,
            protocol_field_type::string,
            protocol_field_type::long_,
            protocol_field_type::date,
            protocol_field_type::double_
        }
    )
), test_name_generator);

// errc cases for deserialize_binary_row
struct binary_row_err_testcase : named_param
{
    std::string name;
    std::vector<std::uint8_t> from;
    errc expected;
    std::vector<protocol_field_type> types;

    binary_row_err_testcase(
        std::string name,
        std::vector<std::uint8_t> from,
        errc expected,
        std::vector<protocol_field_type> types
    ):
        name(std::move(name)),
        from(std::move(from)),
        expected(expected),
        types(std::move(types))
    {
    }
};

struct DeserializeBinaryRowErrorTest : public TestWithParam<binary_row_err_testcase> {};

TEST_P(DeserializeBinaryRowErrorTest, ErrorCondition_ReturnsErrorCode)
{
    auto meta = make_meta(GetParam().types);
    const auto& buffer = GetParam().from;
    deserialization_context ctx (buffer.data(), buffer.data() + buffer.size(), capabilities());

    std::vector<value> actual;
    auto err = deserialize_binary_row(ctx, meta, actual);
    EXPECT_EQ(err, make_error_code(GetParam().expected));
}

INSTANTIATE_TEST_SUITE_P(Default, DeserializeBinaryRowErrorTest, testing::Values(
    binary_row_err_testcase("no_space_null_bitmap_1", {0x00}, errc::incomplete_message, {protocol_field_type::tiny}),
    binary_row_err_testcase("no_space_null_bitmap_2", {0x00, 0xfc}, errc::incomplete_message,
            std::vector<protocol_field_type>(7, protocol_field_type::tiny)),
    binary_row_err_testcase("no_space_value_single", {0x00, 0x00}, errc::incomplete_message, {protocol_field_type::tiny}),
    binary_row_err_testcase("no_space_value_last", {0x00, 0x00, 0x01}, errc::incomplete_message,
            std::vector<protocol_field_type>(2, protocol_field_type::tiny)),
    binary_row_err_testcase("no_space_value_middle", {0x00, 0x00, 0x01}, errc::incomplete_message,
            std::vector<protocol_field_type>(3, protocol_field_type::tiny)),
    binary_row_err_testcase("extra_bytes", {0x00, 0x00, 0x01, 0x02}, errc::extra_bytes, {protocol_field_type::tiny})
), test_name_generator);


} // anon namespace

