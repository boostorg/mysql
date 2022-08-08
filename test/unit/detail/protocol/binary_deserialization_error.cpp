//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Test deserialize_binary_value(), only error cases

#include <boost/mysql/detail/protocol/binary_deserialization.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>
#include "test_common.hpp"
#include <boost/test/data/monomorphic/collection.hpp>
#include <boost/test/data/test_case.hpp>

using namespace boost::mysql::detail;
using namespace boost::mysql::test;
using namespace boost::unit_test;
using boost::mysql::field_view;
using boost::mysql::error_code;
using boost::mysql::errc;

namespace
{

struct binary_value_err_sample
{
    std::string name;
    bytestring from;
    protocol_field_type type;
    std::uint16_t flags;
    errc expected_err;

    binary_value_err_sample(std::string&& name, bytestring&& from, protocol_field_type type,
            std::uint16_t flags=0, errc expected_err=errc::protocol_value_error) :
        name(std::move(name)),
        from(std::move(from)),
        type(type),
        flags(flags),
        expected_err(expected_err)
    {
    }

    binary_value_err_sample(std::string&& name, bytestring&& from, protocol_field_type type,
            errc expected_err) :
        name(std::move(name)),
        from(std::move(from)),
        type(type),
        flags(0),
        expected_err(expected_err)
    {
    }
};

std::ostream& operator<<(std::ostream& os, const binary_value_err_sample& input)
{
    return os << "(type=" << to_string(input.type)
              << ", name=" << input.name
              << ")";
}

void add_int_samples(
    protocol_field_type type,
    unsigned num_bytes,
    std::vector<binary_value_err_sample>& output
)
{
    output.emplace_back(binary_value_err_sample("signed_not_enough_space",
        bytestring(num_bytes, 0x0a), type, errc::incomplete_message));
    output.emplace_back(binary_value_err_sample("unsigned_not_enough_space",
        bytestring(num_bytes, 0x0a), type, column_flags::unsigned_, errc::incomplete_message));
}

void add_bit_samples(
    std::vector<binary_value_err_sample>& output
)
{
    output.emplace_back(binary_value_err_sample("bit_error_deserializing_string_view",
        {0x01}, protocol_field_type::bit, column_flags::unsigned_, errc::incomplete_message));
    output.emplace_back(binary_value_err_sample("bit_string_view_too_short",
        {0x00}, protocol_field_type::bit, column_flags::unsigned_));
    output.emplace_back(binary_value_err_sample("bit_string_view_too_long",
        {0x09, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09}, 
        protocol_field_type::bit, column_flags::unsigned_));
}

void add_float_samples(std::vector<binary_value_err_sample>& output)
{
    output.push_back(binary_value_err_sample("not_enough_space", {0x01, 0x02, 0x03},
            protocol_field_type::float_, errc::incomplete_message));
    output.push_back(binary_value_err_sample("inf", {0x00, 0x00, 0x80, 0x7f},
            protocol_field_type::float_));
    output.push_back(binary_value_err_sample("minus_inf", {0x00, 0x00, 0x80, 0xff},
            protocol_field_type::float_));
    output.push_back(binary_value_err_sample("nan", {0xff, 0xff, 0xff, 0x7f},
            protocol_field_type::float_));
    output.push_back(binary_value_err_sample("minus_nan", {0xff, 0xff, 0xff, 0xff},
            protocol_field_type::float_));
}

void add_double_samples(std::vector<binary_value_err_sample>& output)
{
    output.push_back(binary_value_err_sample("not_enough_space", {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07},
            protocol_field_type::double_, errc::incomplete_message));
    output.push_back(binary_value_err_sample("inf", {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x7f},
            protocol_field_type::double_));
    output.push_back(binary_value_err_sample("minus_inf", {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xff},
            protocol_field_type::double_));
    output.push_back(binary_value_err_sample("nan", {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f},
            protocol_field_type::double_));
    output.push_back(binary_value_err_sample("minus_nan", {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
            protocol_field_type::double_));
}

// Based on correct, regular date {0x04, 0xda, 0x07, 0x03, 0x1c}
void add_date_samples(std::vector<binary_value_err_sample>& output)
{
    output.push_back(binary_value_err_sample("empty", {},
        protocol_field_type::date, errc::incomplete_message));
    output.push_back(binary_value_err_sample("incomplete_year",   {0x04, 0xda},
            protocol_field_type::date, errc::incomplete_message));
    output.push_back(binary_value_err_sample("no_month_day",      {0x04, 0xda, 0x07},
            protocol_field_type::date, errc::incomplete_message));
    output.push_back(binary_value_err_sample("no_day",            {0x04, 0xda, 0x07, 0x03},
            protocol_field_type::date, errc::incomplete_message));
    output.push_back(binary_value_err_sample("invalid_year",      {0x04, 0x10, 0x27, 0x03, 0x1c}, // year 10000
            protocol_field_type::date));
    output.push_back(binary_value_err_sample("invalid_year_max",  {0x04, 0xff, 0xff, 0x03, 0x1c},
            protocol_field_type::date));
    output.push_back(binary_value_err_sample("invalid_month",     {0x04, 0xda, 0x07,   13, 0x1c},
            protocol_field_type::date));
    output.push_back(binary_value_err_sample("invalid_month_max", {0x04, 0xda, 0x07, 0xff, 0x1c},
            protocol_field_type::date));
    output.push_back(binary_value_err_sample("invalid_day",       {0x04, 0xda, 0x07, 0x03,   32},
            protocol_field_type::date));
    output.push_back(binary_value_err_sample("invalid_day_max",   {0x04, 0xda, 0x07, 0x03, 0xff},
            protocol_field_type::date));
    output.push_back(binary_value_err_sample("protocol_max",      {0xff, 0xff, 0xff, 0xff, 0xff},
            protocol_field_type::date));
}

// Based on correct datetime {0x0b, 0xda, 0x07, 0x01, 0x01, 0x17, 0x01, 0x3b, 0x56, 0xc3, 0x0e, 0x00}
void add_datetime_samples(
    protocol_field_type type,
    std::vector<binary_value_err_sample>& output
)
{
    output.push_back(binary_value_err_sample("empty",
        {}, type, errc::incomplete_message));
    output.push_back(binary_value_err_sample("incomplete_date",
        {0x04, 0xda, 0x07, 0x01}, type, errc::incomplete_message));
    output.push_back(binary_value_err_sample("no_hours_mins_secs",
        {0x07, 0xda, 0x07, 0x01, 0x01}, type, errc::incomplete_message));
    output.push_back(binary_value_err_sample("no_mins_secs",
        {0x07, 0xda, 0x07, 0x01, 0x01, 0x17}, type, errc::incomplete_message));
    output.push_back(binary_value_err_sample("no_secs",
        {0x07, 0xda, 0x07, 0x01, 0x01, 0x17, 0x01}, type, errc::incomplete_message));
    output.push_back(binary_value_err_sample("incomplete_micros",
        {0x0b, 0xda, 0x07, 0x01, 0x01, 0x17, 0x01, 0x3b, 0x56, 0xc3, 0x0e},
        type, errc::incomplete_message));
    output.push_back(binary_value_err_sample("invalid_year_d",
        {0x04, 0x10, 0x27, 0x01, 0x01}, type)); // year 10000
    output.push_back(binary_value_err_sample("invalid_year_hms",
        {0x07, 0x10, 0x27, 0x01, 0x01, 0x17, 0x01, 0x3b}, type));
    output.push_back(binary_value_err_sample("invalid_year_hmsu",
        {0x0b, 0x10, 0x27, 0x01, 0x01, 0x17, 0x01, 0x3b, 0x56, 0xc3, 0x0e, 0x00}, type));
    output.push_back(binary_value_err_sample("invalid_year_max_hmsu",
        {0x0b, 0xff, 0xff, 0x01, 0x01, 0x17, 0x01, 0x3b, 0x56, 0xc3, 0x0e, 0x00}, type));
    output.push_back(binary_value_err_sample("invalid_hour_hms",
        {0x07, 0xda, 0x07, 0x01, 0x01,   24, 0x01, 0x3b}, type));
    output.push_back(binary_value_err_sample("invalid_hour_hmsu",
        {0x0b, 0xda, 0x07, 0x01, 0x01,   24, 0x01, 0x3b, 0x56, 0xc3, 0x0e, 0x00}, type));
    output.push_back(binary_value_err_sample("invalid_hour_max_hmsu",
        {0x0b, 0xda, 0x07, 0x01, 0x01, 0xff, 0x01, 0x3b, 0x56, 0xc3, 0x0e, 0x00}, type));
    output.push_back(binary_value_err_sample("invalid_min_hms",
        {0x07, 0xda, 0x07, 0x01, 0x01, 0x17,   60, 0x3b}, type));
    output.push_back(binary_value_err_sample("invalid_min_hmsu",
        {0x0b, 0xda, 0x07, 0x01, 0x01, 0x17,   60, 0x3b, 0x56, 0xc3, 0x0e, 0x00}, type));
    output.push_back(binary_value_err_sample("invalid_min_max_hmsu",
        {0x0b, 0xda, 0x07, 0x01, 0x01, 0x17, 0xff, 0x3b, 0x56, 0xc3, 0x0e, 0x00}, type));
    output.push_back(binary_value_err_sample("invalid_sec_hms",
        {0x07, 0xda, 0x07, 0x01, 0x01, 0x17, 0x01,   60}, type));
    output.push_back(binary_value_err_sample("invalid_sec_hmsu",
        {0x0b, 0xda, 0x07, 0x01, 0x01, 0x17, 0x01,   60, 0x56, 0xc3, 0x0e, 0x00}, type));
    output.push_back(binary_value_err_sample("invalid_sec_max_hmsu",
        {0x0b, 0xda, 0x07, 0x01, 0x01, 0x17, 0x01, 0xff, 0x56, 0xc3, 0x0e, 0x00}, type));
    output.push_back(binary_value_err_sample("invalid_micro_hmsu",
        {0x0b, 0xda, 0x07, 0x01, 0x01, 0x17, 0x01, 0x3b, 0x40, 0x42, 0xf4, 0x00}, type)); // 1M
    output.push_back(binary_value_err_sample("invalid_micro_max_hmsu",
        {0x0b, 0xda, 0x07, 0x01, 0x01, 0x17, 0x01, 0x3b, 0xff, 0xff, 0xff, 0xff}, type));
    output.push_back(binary_value_err_sample("invalid_hour_invalid_date",
        {0x0b, 0x00, 0x00, 0x00, 0x00, 0xff, 0x01, 0x3b, 0x56, 0xc3, 0x0e, 0x00}, type));
    output.push_back(binary_value_err_sample("invalid_min_invalid_date",
        {0x0b, 0x00, 0x00, 0x00, 0x00, 0x17, 0xff, 0x3b, 0x56, 0xc3, 0x0e, 0x00}, type));
    output.push_back(binary_value_err_sample("invalid_sec_invalid_date",
        {0x0b, 0x00, 0x00, 0x00, 0x00, 0x17, 0x01, 0xff, 0x56, 0xc3, 0x0e, 0x00}, type));
    output.push_back(binary_value_err_sample("invalid_micro_invalid_date",
        {0x0b, 0x00, 0x00, 0x00, 0x00, 0x17, 0x01, 0x3b, 0xff, 0xff, 0xff, 0xff}, type));
    output.push_back(binary_value_err_sample("protocol_max",
        {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, type));
}

void add_time_samples(std::vector<binary_value_err_sample>& output)
{
    constexpr auto type = protocol_field_type::time;
    output.push_back(binary_value_err_sample("empty",
        {}, type, errc::incomplete_message));
    output.push_back(binary_value_err_sample("no_sign_days_hours_mins_secs",
        {0x08}, type, errc::incomplete_message));
    output.push_back(binary_value_err_sample("no_days_hours_mins_secs",
        {0x08, 0x01}, type, errc::incomplete_message));
    output.push_back(binary_value_err_sample("no_hours_mins_secs",
        {0x08, 0x01, 0x22, 0x00, 0x00, 0x00}, type, errc::incomplete_message));
    output.push_back(binary_value_err_sample("no_mins_secs",
        {0x08, 0x01, 0x22, 0x00, 0x00, 0x00, 0x16}, type, errc::incomplete_message));
    output.push_back(binary_value_err_sample("no_secs",
        {0x08, 0x01, 0x22, 0x00, 0x00, 0x00, 0x16, 0x3b}, type, errc::incomplete_message));
    output.push_back(binary_value_err_sample("no_micros",
        {0x0c, 0x01, 0x22, 0x00, 0x00, 0x00, 0x16, 0x3b, 0x3a}, type, errc::incomplete_message));

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
        output.emplace_back(c.first + std::string("_positive"), bytestring(c.second), type);

        // Negative
        c.second[1] = 0x01;
        output.emplace_back(c.first + std::string("_negative"), std::move(c.second), type);
    }
}

std::vector<binary_value_err_sample> make_all_samples()
{
    std::vector<binary_value_err_sample> res;
    add_int_samples(protocol_field_type::tiny, 0, res);
    add_int_samples(protocol_field_type::short_, 1, res);
    add_int_samples(protocol_field_type::int24, 3, res);
    add_int_samples(protocol_field_type::long_, 3, res);
    add_int_samples(protocol_field_type::longlong, 7, res);
    add_int_samples(protocol_field_type::year, 1, res);
    add_bit_samples(res);
    add_float_samples(res);
    add_double_samples(res);
    add_date_samples(res);
    add_datetime_samples(protocol_field_type::datetime, res);
    add_datetime_samples(protocol_field_type::timestamp, res);
    add_time_samples(res);
    return res;
}

BOOST_DATA_TEST_CASE(test_deserialize_binary_value_error, data::make(make_all_samples()))
{
    column_definition_packet coldef {};
    coldef.type = sample.type;
    coldef.flags = sample.flags;
    boost::mysql::metadata meta (coldef);
    field_view actual_value;
    const auto& buff = sample.from;
    deserialization_context ctx (buff.data(), buff.data() + buff.size(), capabilities());
    auto err = deserialize_binary_value(ctx, meta, actual_value);
    auto expected = sample.expected_err;
    BOOST_TEST(expected == err);
}

} // anon namespace
