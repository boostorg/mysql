//
// Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Tests for both deserialize_binary_row() and deserialize_text_row()

#include "boost/mysql/detail/protocol/text_deserialization.hpp"
#include "boost/mysql/detail/protocol/binary_deserialization.hpp"
#include "boost/mysql/detail/network_algorithms/common.hpp" // for deserialize_row_fn
#include "test_common.hpp"
#include <boost/test/data/monomorphic/collection.hpp>
#include <boost/test/data/test_case.hpp>

using namespace boost::mysql::detail;
using namespace boost::mysql::test;
using namespace boost::unit_test;
using boost::mysql::value;
using boost::mysql::collation;
using boost::mysql::error_code;
using boost::mysql::errc;
using boost::mysql::field_metadata;

BOOST_AUTO_TEST_SUITE(test_row_deserialization)

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

constexpr auto text = &deserialize_text_row;
constexpr auto bin =  &deserialize_binary_row;

// Success cases
struct row_sample
{
    std::string name;
    deserialize_row_fn deserializer;
    std::vector<std::uint8_t> from;
    std::vector<value> expected;
    std::vector<field_metadata> meta;

    row_sample(
        deserialize_row_fn deserializer,
        std::string name,
        std::vector<std::uint8_t>&& from,
        std::vector<value> expected,
        const std::vector<protocol_field_type>& types
    ) :
        name(std::move(name)),
        deserializer(deserializer),
        from(std::move(from)),
        expected(std::move(expected)),
        meta(make_meta(types))
    {
        assert(this->expected.size() == this->meta.size());
    }
};

std::ostream& operator<<(std::ostream& os, const row_sample& input)
{
    return os << "(type=" << (input.deserializer == text ? "text" : "binary")
              << ", name=" << input.name << ")";
}

std::vector<row_sample> make_ok_samples()
{
    return {
        // text
        row_sample(
            text,
            "one_value",
            {0x01, 0x35},
            make_value_vector(std::int64_t(5)),
            { protocol_field_type::tiny }
        ),
        row_sample(
            text,
            "one_null",
            {0xfb},
            make_value_vector(nullptr),
            { protocol_field_type::tiny }
        ),
        row_sample(
            text,
            "several_values",
            {0x03, 0x76, 0x61, 0x6c, 0x02, 0x32, 0x31, 0x03, 0x30, 0x2e, 0x30},
            make_value_vector("val", std::int64_t(21), 0.0f),
            { protocol_field_type::var_string, protocol_field_type::long_, protocol_field_type::float_ }
        ),
        row_sample(
            text,
            "several_values_one_null",
            {0x03, 0x76, 0x61, 0x6c, 0xfb, 0x03, 0x30, 0x2e, 0x30},
            make_value_vector("val", nullptr, 0.0f),
            { protocol_field_type::var_string, protocol_field_type::long_, protocol_field_type::float_ }
        ),
        row_sample(
            text,
            "several_nulls",
            {0xfb, 0xfb, 0xfb},
            make_value_vector(nullptr, nullptr, nullptr),
            { protocol_field_type::var_string, protocol_field_type::long_, protocol_field_type::datetime }
        ),

        // binary
        row_sample(bin, "one_value", {0x00, 0x00, 0x14},
                make_value_vector(std::int64_t(20)),
                {protocol_field_type::tiny}),
        row_sample(bin, "one_null", {0x00, 0x04},
                make_value_vector(nullptr),
                {protocol_field_type::tiny}),
        row_sample(bin, "two_values", {0x00, 0x00, 0x03, 0x6d, 0x69, 0x6e, 0x6d, 0x07},
                make_value_vector("min", std::int64_t(1901)),
                {protocol_field_type::var_string, protocol_field_type::short_}),
        row_sample(bin, "one_value_one_null", {0x00, 0x08, 0x03, 0x6d, 0x61, 0x78},
                make_value_vector("max", nullptr),
                {protocol_field_type::var_string, protocol_field_type::tiny}),
        row_sample(bin, "two_nulls", {0x00, 0x0c},
                make_value_vector(nullptr, nullptr),
                {protocol_field_type::tiny, protocol_field_type::tiny}),
        row_sample(bin, "six_nulls", {0x00, 0xfc},
                std::vector<value>(6, value()),
                std::vector<protocol_field_type>(6, protocol_field_type::tiny)),
        row_sample(bin, "seven_nulls", {0x00, 0xfc, 0x01},
                std::vector<value>(7, value()),
                std::vector<protocol_field_type>(7, protocol_field_type::tiny)),
        row_sample(bin, "several_values", {
                0x00, 0x90, 0x00, 0xfd, 0x14, 0x00, 0xc3, 0xf5, 0x48,
                0x40, 0x02, 0x61, 0x62, 0x04, 0xe2, 0x07, 0x0a,
                0x05, 0x71, 0x99, 0x6d, 0xe2, 0x93, 0x4d, 0xf5,
                0x3d
            }, make_value_vector(
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
    };
}

BOOST_DATA_TEST_CASE(deserialize_row_ok, data::make(make_ok_samples()))
{
    const auto& buffer = sample.from;
    deserialization_context ctx (buffer.data(), buffer.data() + buffer.size(), capabilities());

    std::vector<value> actual;
    auto err = sample.deserializer(ctx, sample.meta, actual);
    BOOST_TEST(err == error_code());
    BOOST_TEST(actual == sample.expected);
}

// Error cases
struct row_err_sample
{
    std::string name;
    deserialize_row_fn deserializer;
    std::vector<std::uint8_t> from;
    errc expected;
    std::vector<field_metadata> meta;

    row_err_sample(
        deserialize_row_fn deserializer,
        std::string name,
        std::vector<std::uint8_t>&& from,
        errc expected,
        std::vector<protocol_field_type> types
    ):
        name(std::move(name)),
        deserializer(deserializer),
        from(std::move(from)),
        expected(expected),
        meta(make_meta(types))
    {
    }
};

std::ostream& operator<<(std::ostream& os, const row_err_sample& input)
{
    return os << "(type=" << (input.deserializer == text ? "text" : "binary")
              << ", name=" << input.name << ")";
}

std::vector<row_err_sample> make_err_samples()
{
    return {
        // text
        row_err_sample(text, "no_space_string_single", {0x02, 0x00}, errc::incomplete_message,
                {protocol_field_type::short_}),
        row_err_sample(text, "no_space_string_final", {0x01, 0x35, 0x02, 0x35}, errc::incomplete_message,
                {protocol_field_type::tiny, protocol_field_type::short_}),
        row_err_sample(text, "no_space_null_single", {}, errc::incomplete_message,
                {protocol_field_type::tiny}),
        row_err_sample(text, "no_space_null_final", {0xfb}, errc::incomplete_message,
                {protocol_field_type::tiny, protocol_field_type::tiny}),
        row_err_sample(text, "extra_bytes", {0x01, 0x35, 0xfb, 0x00}, errc::extra_bytes,
                {protocol_field_type::tiny, protocol_field_type::tiny}),
        row_err_sample(text, "contained_value_error_single", {0x01, 0x00}, errc::protocol_value_error,
                {protocol_field_type::date}),
        row_err_sample(text, "contained_value_error_middle", {0xfb, 0x01, 0x00, 0xfb}, errc::protocol_value_error,
                {protocol_field_type::date, protocol_field_type::date, protocol_field_type::date}),

        // binary
        row_err_sample(bin, "no_space_null_bitmap_1", {0x00},
                errc::incomplete_message,
                {protocol_field_type::tiny}),
        row_err_sample(bin, "no_space_null_bitmap_2", {0x00, 0xfc},
                errc::incomplete_message,
                std::vector<protocol_field_type>(7, protocol_field_type::tiny)),
        row_err_sample(bin, "no_space_value_single", {0x00, 0x00},
                errc::incomplete_message,
                {protocol_field_type::tiny}),
        row_err_sample(bin, "no_space_value_last", {0x00, 0x00, 0x01},
                errc::incomplete_message,
                std::vector<protocol_field_type>(2, protocol_field_type::tiny)),
        row_err_sample(bin, "no_space_value_middle", {0x00, 0x00, 0x01},
                errc::incomplete_message,
                std::vector<protocol_field_type>(3, protocol_field_type::tiny)),
        row_err_sample(bin, "extra_bytes", {0x00, 0x00, 0x01, 0x02},
                errc::extra_bytes,
                {protocol_field_type::tiny})
    };
}


BOOST_DATA_TEST_CASE(deserialize_row_error, data::make(make_err_samples()))
{
    const auto& buffer = sample.from;
    deserialization_context ctx (buffer.data(), buffer.data() + buffer.size(), capabilities());

    std::vector<value> actual;
    auto err = sample.deserializer(ctx, sample.meta, actual);
    BOOST_TEST(err == make_error_code(sample.expected));
}


BOOST_AUTO_TEST_SUITE_END() // test_row_deserialization

