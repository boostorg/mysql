//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Tests for both deserialize_binary_row() and deserialize_text_row()

#include <gtest/gtest.h>
#include "boost/mysql/detail/protocol/text_deserialization.hpp"
#include "boost/mysql/detail/protocol/binary_deserialization.hpp"
#include "boost/mysql/detail/network_algorithms/common.hpp" // for deserialize_row_fn
#include "test_common.hpp"

using namespace boost::mysql::detail;
using namespace boost::mysql::test;
using namespace testing;
using boost::mysql::value;
using boost::mysql::collation;
using boost::mysql::error_code;
using boost::mysql::errc;
using boost::mysql::field_metadata;

namespace
{

// Common
std::vector<field_metadata> make_meta(
    const std::vector<protocol_field_type>& types
)
{
    std::vector<boost::mysql::field_metadata> res;
    for (const auto type: types)
    {
        column_definition_packet coldef {};
        coldef.type = type;
        res.emplace_back(coldef);
    }
    return res;
}

// Success cases
struct row_testcase : public named
{
    deserialize_row_fn deserializer;
    std::vector<std::uint8_t> from;
    std::vector<value> expected;
    std::vector<field_metadata> meta;

    row_testcase(
        deserialize_row_fn deserializer,
        std::string name,
        std::vector<std::uint8_t>&& from,
        std::vector<value> expected,
        const std::vector<protocol_field_type>& types
    ):
        named(std::move(name)),
        deserializer(deserializer),
        from(std::move(from)),
        expected(std::move(expected)),
        meta(make_meta(types))
    {
        assert(this->expected.size() == this->meta.size());
    }
};

struct DeserializeRowTest : public TestWithParam<row_testcase>
{
};

TEST_P(DeserializeRowTest, CorrectFormat_SetsOutputValueReturnsTrue)
{
    const auto& buffer = GetParam().from;
    deserialization_context ctx (buffer.data(), buffer.data() + buffer.size(), capabilities());

    std::vector<value> actual;
    auto err = GetParam().deserializer(ctx, GetParam().meta, actual);
    EXPECT_EQ(err, error_code());
    EXPECT_EQ(actual, GetParam().expected);
}

// Error cases
struct row_err_testcase : public named
{
    deserialize_row_fn deserializer;
    std::vector<std::uint8_t> from;
    errc expected;
    std::vector<field_metadata> meta;

    row_err_testcase(
        deserialize_row_fn deserializer,
        std::string name,
        std::vector<std::uint8_t>&& from,
        errc expected,
        std::vector<protocol_field_type> types
    ):
        named(std::move(name)),
        deserializer(deserializer),
        from(std::move(from)),
        expected(expected),
        meta(make_meta(types))
    {
    }
};

struct DeserializeRowErrorTest : public TestWithParam<row_err_testcase>
{
};

TEST_P(DeserializeRowErrorTest, ErrorCondition_ReturnsErrorCode)
{
    const auto& buffer = GetParam().from;
    deserialization_context ctx (buffer.data(), buffer.data() + buffer.size(), capabilities());

    std::vector<value> actual;
    auto err = GetParam().deserializer(ctx, GetParam().meta, actual);
    EXPECT_EQ(err, make_error_code(GetParam().expected));
}

// Helpers to make things less verbose
constexpr auto text = &deserialize_text_row;
constexpr auto bin =  &deserialize_binary_row;

// Instantiations for text protocol
INSTANTIATE_TEST_SUITE_P(Text, DeserializeRowTest, Values(
    row_testcase(
        text,
        "one_value",
        {0x01, 0x35},
        makevalues(std::int64_t(5)),
        { protocol_field_type::tiny }
    ),
    row_testcase(
        text,
        "one_null",
        {0xfb},
        makevalues(nullptr),
        { protocol_field_type::tiny }
    ),
    row_testcase(
        text,
        "several_values",
        {0x03, 0x76, 0x61, 0x6c, 0x02, 0x32, 0x31, 0x03, 0x30, 0x2e, 0x30},
        makevalues("val", std::int64_t(21), 0.0f),
        { protocol_field_type::var_string, protocol_field_type::long_, protocol_field_type::float_ }
    ),
    row_testcase(
        text,
        "several_values_one_null",
        {0x03, 0x76, 0x61, 0x6c, 0xfb, 0x03, 0x30, 0x2e, 0x30},
        makevalues("val", nullptr, 0.0f),
        { protocol_field_type::var_string, protocol_field_type::long_, protocol_field_type::float_ }
    ),
    row_testcase(
        text,
        "several_nulls",
        {0xfb, 0xfb, 0xfb},
        makevalues(nullptr, nullptr, nullptr),
        { protocol_field_type::var_string, protocol_field_type::long_, protocol_field_type::datetime }
    )
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(Text, DeserializeRowErrorTest, testing::Values(
    row_err_testcase(text, "no_space_string_single", {0x02, 0x00}, errc::incomplete_message,
            {protocol_field_type::short_}),
    row_err_testcase(text, "no_space_string_final", {0x01, 0x35, 0x02, 0x35}, errc::incomplete_message,
            {protocol_field_type::tiny, protocol_field_type::short_}),
    row_err_testcase(text, "no_space_null_single", {}, errc::incomplete_message,
            {protocol_field_type::tiny}),
    row_err_testcase(text, "no_space_null_final", {0xfb}, errc::incomplete_message,
            {protocol_field_type::tiny, protocol_field_type::tiny}),
    row_err_testcase(text, "extra_bytes", {0x01, 0x35, 0xfb, 0x00}, errc::extra_bytes,
            {protocol_field_type::tiny, protocol_field_type::tiny}),
    row_err_testcase(text, "contained_value_error_single", {0x01, 0x00}, errc::protocol_value_error,
            {protocol_field_type::date}),
    row_err_testcase(text, "contained_value_error_middle", {0xfb, 0x01, 0x00, 0xfb}, errc::protocol_value_error,
            {protocol_field_type::date, protocol_field_type::date, protocol_field_type::date})
), test_name_generator);

// Instantiations for binary protocol
INSTANTIATE_TEST_SUITE_P(Binary, DeserializeRowTest, testing::Values(
    row_testcase(bin, "one_value", {0x00, 0x00, 0x14},
            makevalues(std::int64_t(20)),
            {protocol_field_type::tiny}),
    row_testcase(bin, "one_null", {0x00, 0x04},
            makevalues(nullptr),
            {protocol_field_type::tiny}),
    row_testcase(bin, "two_values", {0x00, 0x00, 0x03, 0x6d, 0x69, 0x6e, 0x6d, 0x07},
            makevalues("min", std::int64_t(1901)),
            {protocol_field_type::var_string, protocol_field_type::short_}),
    row_testcase(bin, "one_value_one_null", {0x00, 0x08, 0x03, 0x6d, 0x61, 0x78},
            makevalues("max", nullptr),
            {protocol_field_type::var_string, protocol_field_type::tiny}),
    row_testcase(bin, "two_nulls", {0x00, 0x0c},
            makevalues(nullptr, nullptr),
            {protocol_field_type::tiny, protocol_field_type::tiny}),
    row_testcase(bin, "six_nulls", {0x00, 0xfc},
            std::vector<value>(6, value(nullptr)),
            std::vector<protocol_field_type>(6, protocol_field_type::tiny)),
    row_testcase(bin, "seven_nulls", {0x00, 0xfc, 0x01},
            std::vector<value>(7, value(nullptr)),
            std::vector<protocol_field_type>(7, protocol_field_type::tiny)),
    row_testcase(bin, "several_values", {
            0x00, 0x90, 0x00, 0xfd, 0x14, 0x00, 0xc3, 0xf5, 0x48,
            0x40, 0x02, 0x61, 0x62, 0x04, 0xe2, 0x07, 0x0a,
            0x05, 0x71, 0x99, 0x6d, 0xe2, 0x93, 0x4d, 0xf5,
            0x3d
        }, makevalues(
            std::int64_t(-3),
            std::int64_t(20),
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

INSTANTIATE_TEST_SUITE_P(Binary, DeserializeRowErrorTest, testing::Values(
    row_err_testcase(bin, "no_space_null_bitmap_1", {0x00},
            errc::incomplete_message,
            {protocol_field_type::tiny}),
    row_err_testcase(bin, "no_space_null_bitmap_2", {0x00, 0xfc},
            errc::incomplete_message,
            std::vector<protocol_field_type>(7, protocol_field_type::tiny)),
    row_err_testcase(bin, "no_space_value_single", {0x00, 0x00},
            errc::incomplete_message,
            {protocol_field_type::tiny}),
    row_err_testcase(bin, "no_space_value_last", {0x00, 0x00, 0x01},
            errc::incomplete_message,
            std::vector<protocol_field_type>(2, protocol_field_type::tiny)),
    row_err_testcase(bin, "no_space_value_middle", {0x00, 0x00, 0x01},
            errc::incomplete_message,
            std::vector<protocol_field_type>(3, protocol_field_type::tiny)),
    row_err_testcase(bin, "extra_bytes", {0x00, 0x00, 0x01, 0x02},
            errc::extra_bytes,
            {protocol_field_type::tiny})
), test_name_generator);


} // anon namespace

