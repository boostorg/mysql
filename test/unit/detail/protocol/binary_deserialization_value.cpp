//
// Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/detail/protocol/binary_deserialization.hpp>
#include "test_common.hpp"
#include <boost/test/data/monomorphic/collection.hpp>
#include <boost/test/data/test_case.hpp>

// Tests for deserialize_binary_value()

using namespace boost::mysql::detail;
using namespace boost::mysql::test;
using namespace boost::unit_test;
using boost::mysql::value;
using boost::mysql::error_code;
using boost::mysql::errc;

namespace
{

struct binary_value_sample
{
    std::string name;
    std::vector<std::uint8_t> from;
    value expected;
    protocol_field_type type;
    std::uint16_t flags;

    template <class T>
    binary_value_sample(
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

std::ostream& operator<<(std::ostream& os, const binary_value_sample& input)
{
    return os << "(type=" << to_string(input.type)
              << ", name=" << input.name
              << ")";
}

void add_string_samples(std::vector<binary_value_sample>& output)
{
    output.push_back(binary_value_sample("varchar",
        {0x04, 0x74, 0x65, 0x73, 0x74}, "test", protocol_field_type::var_string));
    output.push_back(binary_value_sample("char",
        {0x04, 0x74, 0x65, 0x73, 0x74}, "test", protocol_field_type::string));
    output.push_back(binary_value_sample("varbinary",
        {0x04, 0x74, 0x65, 0x73, 0x74}, "test", protocol_field_type::var_string, column_flags::binary));
    output.push_back(binary_value_sample("binary",
        {0x04, 0x74, 0x65, 0x73, 0x74}, "test", protocol_field_type::string, column_flags::binary));
    output.push_back(binary_value_sample("text_blob",
        {0x04, 0x74, 0x65, 0x73, 0x74}, "test", protocol_field_type::blob, column_flags::blob));
    output.push_back(binary_value_sample("enum",
        {0x04, 0x74, 0x65, 0x73, 0x74}, "test", protocol_field_type::string, column_flags::enum_));
    output.push_back(binary_value_sample("set",
        {0x04, 0x74, 0x65, 0x73, 0x74}, "test", protocol_field_type::string, column_flags::set));
    output.push_back(binary_value_sample("decimal",
        {0x02, 0x31, 0x30}, "10", protocol_field_type::newdecimal));
    output.push_back(binary_value_sample("geomtry",
        {0x04, 0x74, 0x65, 0x73, 0x74}, "test", protocol_field_type::geometry));

    // Anything we don't know what it is, we interpret as a string
    output.push_back(binary_value_sample("unknown_protocol_type",
        {0x04, 0x74, 0x65, 0x73, 0x74}, "test", static_cast<protocol_field_type>(0x23)));
}

// Note: these employ regular integer deserialization functions, which have
// already been tested
void add_int_samples(std::vector<binary_value_sample>& output)
{
    output.push_back(binary_value_sample("tinyint_unsigned",
        {0x14}, std::uint64_t(20), protocol_field_type::tiny, column_flags::unsigned_));
    output.push_back(binary_value_sample("tinyint_signed",
        {0xec}, std::int64_t(-20), protocol_field_type::tiny));

    output.push_back(binary_value_sample("smallint_unsigned",
        {0x14, 0x00}, std::uint64_t(20), protocol_field_type::short_, column_flags::unsigned_));
    output.push_back(binary_value_sample("smallint_signed",
        {0xec, 0xff}, std::int64_t(-20), protocol_field_type::short_));

    output.push_back(binary_value_sample("mediumint_unsigned",
        {0x14, 0x00, 0x00, 0x00}, std::uint64_t(20),
        protocol_field_type::int24, column_flags::unsigned_));
    output.push_back(binary_value_sample("mediumint_signed",
        {0xec, 0xff, 0xff, 0xff}, std::int64_t(-20),
        protocol_field_type::int24));

    output.push_back(binary_value_sample("int_unsigned",
        {0x14, 0x00, 0x00, 0x00}, std::uint64_t(20),
        protocol_field_type::long_, column_flags::unsigned_));
    output.push_back(binary_value_sample("int_signed", {
        0xec, 0xff, 0xff, 0xff}, std::int64_t(-20),
        protocol_field_type::long_));

    output.push_back(binary_value_sample("bigint_unsigned",
        {0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, std::uint64_t(20),
        protocol_field_type::longlong, column_flags::unsigned_));
    output.push_back(binary_value_sample("bigint_signed",
        {0xec, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, std::int64_t(-20),
        protocol_field_type::longlong));

    output.push_back(binary_value_sample("year", {0xe3, 0x07}, std::uint64_t(2019),
        protocol_field_type::year, column_flags::unsigned_));
}

// bit
void add_bit_types(std::vector<binary_value_sample>& output)
{
    output.push_back(binary_value_sample("bit_8",
        {0x01, 0x12}, std::uint64_t(0x12),
        protocol_field_type::bit, column_flags::unsigned_));
    output.push_back(binary_value_sample("bit_16",
        {0x02, 0x12, 0x34}, std::uint64_t(0x1234),
        protocol_field_type::bit, column_flags::unsigned_));
    output.push_back(binary_value_sample("bit_24",
        {0x03, 0x12, 0x34, 0x56}, std::uint64_t(0x123456),
        protocol_field_type::bit, column_flags::unsigned_));
    output.push_back(binary_value_sample("bit_32",
        {0x04, 0x12, 0x34, 0x56, 0x78}, std::uint64_t(0x12345678),
        protocol_field_type::bit, column_flags::unsigned_));
    output.push_back(binary_value_sample("bit_40",
        {0x05, 0x12, 0x34, 0x56, 0x78, 0x9a}, std::uint64_t(0x123456789a),
        protocol_field_type::bit, column_flags::unsigned_));
    output.push_back(binary_value_sample("bit_48",
        {0x06, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc}, std::uint64_t(0x123456789abc),
        protocol_field_type::bit, column_flags::unsigned_));
    output.push_back(binary_value_sample("bit_56",
        {0x07, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde}, std::uint64_t(0x123456789abcde),
        protocol_field_type::bit, column_flags::unsigned_));
    output.push_back(binary_value_sample("bit_64",
        {0x08, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}, std::uint64_t(0x123456789abcdef0),
        protocol_field_type::bit, column_flags::unsigned_));
}

void add_float_samples(std::vector<binary_value_sample>& output)
{
    output.push_back(binary_value_sample("fractional_negative", {0x66, 0x66, 0x86, 0xc0},
            -4.2f, protocol_field_type::float_));
    output.push_back(binary_value_sample("fractional_positive", {0x66, 0x66, 0x86, 0x40},
            4.2f, protocol_field_type::float_));
    output.push_back(binary_value_sample("positive_exp_positive_fractional", {0x01, 0x2d, 0x88, 0x61},
            3.14e20f, protocol_field_type::float_));
    output.push_back(binary_value_sample("zero", {0x00, 0x00, 0x00, 0x00},
            0.0f, protocol_field_type::float_));
}

void add_double_samples(std::vector<binary_value_sample>& output)
{
    output.push_back(binary_value_sample("fractional_negative",
        {0xcd, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x10, 0xc0},
        -4.2, protocol_field_type::double_));
    output.push_back(binary_value_sample("fractional_positive",
        {0xcd, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x10, 0x40},
        4.2, protocol_field_type::double_));
    output.push_back(binary_value_sample("positive_exp_positive_fractional",
        {0xce, 0x46, 0x3c, 0x76, 0x9c, 0x68, 0x90, 0x69},
        3.14e200, protocol_field_type::double_));
    output.push_back(binary_value_sample("zero",
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        0.0, protocol_field_type::double_));
}

void add_date_samples(std::vector<binary_value_sample>& output)
{
    output.push_back(binary_value_sample("regular", {0x04, 0xda, 0x07, 0x03, 0x1c},
            makedate(2010, 3, 28), protocol_field_type::date));
    output.push_back(binary_value_sample("min", {0x04, 0x00, 0x00, 0x01, 0x01},
            makedate(0, 1, 1), protocol_field_type::date));
    output.push_back(binary_value_sample("max", {0x04, 0x0f, 0x27, 0x0c, 0x1f},
            makedate(9999, 12, 31), protocol_field_type::date));
    output.push_back(binary_value_sample("zero", {0x00},
            nullptr, protocol_field_type::date));
    output.push_back(binary_value_sample("zero_full_length", {0x04, 0x00, 0x00, 0x00, 0x00},
            nullptr, protocol_field_type::date));
    output.push_back(binary_value_sample("zero_month",   {0x04, 0x00, 0x00, 0x00, 0x01},
            nullptr, protocol_field_type::date));
    output.push_back(binary_value_sample("zero_day",     {0x04, 0x00, 0x00, 0x01, 0x00},
            nullptr, protocol_field_type::date));
    output.push_back(binary_value_sample("zero_month_day_nonzero_year", {0x04, 0x01, 0x00, 0x00, 0x00},
            nullptr, protocol_field_type::date));
    output.push_back(binary_value_sample("invalid_date", {0x04, 0x00, 0x00,   11,   31},
            nullptr, protocol_field_type::date));
}

void add_datetime_samples(
    protocol_field_type type,
    std::vector<binary_value_sample>& output
)
{
    output.push_back(binary_value_sample("only_date",
        {0x04, 0xda, 0x07, 0x01, 0x01},
        makedt(2010, 1, 1), type));
    output.push_back(binary_value_sample("date_h",
        {0x07, 0xda, 0x07, 0x01, 0x01, 0x14, 0x00, 0x00},
        makedt(2010, 1, 1, 20,  0,  0,  0), type));
    output.push_back(binary_value_sample("date_m",
        {0x07, 0xda, 0x07, 0x01, 0x01, 0x00, 0x01, 0x00},
        makedt(2010, 1, 1, 0,   1,  0,  0), type));
    output.push_back(binary_value_sample("date_hm",
        {0x07, 0xda, 0x07, 0x01, 0x01, 0x03, 0x02, 0x00},
        makedt(2010, 1, 1, 3,   2,  0,  0), type));
    output.push_back(binary_value_sample("date_s",
        {0x07, 0xda, 0x07, 0x01, 0x01, 0x00, 0x00, 0x01},
        makedt(2010, 1, 1, 0,   0,  1,  0), type));
    output.push_back(binary_value_sample("date_ms",
        {0x07, 0xda, 0x07, 0x01, 0x01, 0x00, 0x3b, 0x01},
        makedt(2010, 1, 1, 0,   59, 1,  0), type));
    output.push_back(binary_value_sample("date_hs",
        {0x07, 0xda, 0x07, 0x01, 0x01, 0x05, 0x00, 0x01},
        makedt(2010, 1, 1, 5,   0,  1,  0), type));
    output.push_back(binary_value_sample("date_hms",
        {0x07, 0xda, 0x07, 0x01, 0x01, 0x17, 0x01, 0x3b},
        makedt(2010, 1, 1, 23,  1,  59, 0), type));
    output.push_back(binary_value_sample("date_u",
        {0x0b, 0xda, 0x07, 0x01, 0x01, 0x00, 0x00, 0x00, 0x78, 0xd4, 0x03, 0x00},
        makedt(2010, 1, 1, 0,   0,  0,  251000), type));
    output.push_back(binary_value_sample("date_hu",
        {0x0b, 0xda, 0x07, 0x01, 0x01, 0x17, 0x00, 0x00, 0x56, 0xc3, 0x0e, 0x00},
        makedt(2010, 1, 1, 23,  0,  0,  967510), type));
    output.push_back(binary_value_sample("date_mu",
        {0x0b, 0xda, 0x07, 0x01, 0x01, 0x00, 0x01, 0x00, 0x56, 0xc3, 0x0e, 0x00},
        makedt(2010, 1, 1, 0,   1,  0,  967510), type));
    output.push_back(binary_value_sample("date_hmu",
        {0x0b, 0xda, 0x07, 0x01, 0x01, 0x17, 0x01, 0x00, 0x56, 0xc3, 0x0e, 0x00},
        makedt(2010, 1, 1, 23,  1,  0,  967510), type));
    output.push_back(binary_value_sample("date_su",
        {0x0b, 0xda, 0x07, 0x01, 0x01, 0x00, 0x00, 0x3b, 0x56, 0xc3, 0x0e, 0x00},
        makedt(2010, 1, 1, 0,   0,  59, 967510), type));
    output.push_back(binary_value_sample("date_msu",
        {0x0b, 0xda, 0x07, 0x01, 0x01, 0x00, 0x01, 0x3b, 0x56, 0xc3, 0x0e, 0x00},
        makedt(2010, 1, 1, 0,   1,  59, 967510), type));
    output.push_back(binary_value_sample("date_hsu",
        {0x0b, 0xda, 0x07, 0x01, 0x01, 0x17, 0x00, 0x3b, 0x56, 0xc3, 0x0e, 0x00},
        makedt(2010, 1, 1, 23,  0,  59, 967510), type));
    output.push_back(binary_value_sample("date_hmsu",
        {0x0b, 0xda, 0x07, 0x01, 0x01, 0x17, 0x01, 0x3b, 0x56, 0xc3, 0x0e, 0x00},
        makedt(2010, 1, 1, 23,  1,  59, 967510), type));
    output.push_back(binary_value_sample("zeros", {0x00}, nullptr, type));

    // Create all the casuistic for datetimes with invalid dates. For all possible lengths,
    // try invalid date, zero month, zero day, zero date
    constexpr struct
    {
        const char* name;
        std::uint8_t length;
    } lengths [] = {
        { "d",    4 },
        { "hms",  7 },
        { "hmsu", 11 }
    };

    const struct
    {
        const char* name;
        void (*invalidator)(bytestring&);
    } why_is_invalid [] = {
        { "zeros",          [](bytestring& b) { std::memset(b.data() + 1, 0, b.size() - 1); } },
        { "invalid_date",   [](bytestring& b) { b[3] = 11; b[4] = 31; } },
        { "zero_month",     [](bytestring& b) { b[3] = 0; } },
        { "zero_day",       [](bytestring& b) { b[4] = 0; } },
        { "zero_month_day", [](bytestring& b) { std::memset(b.data()+1, 0, 4); } },
    };

    // Template datetime
    bytestring regular {0x0b, 0xda, 0x07, 0x01, 0x01, 0x17, 0x01, 0x3b, 0x56, 0xc3, 0x0e, 0x00};

    for (const auto& why: why_is_invalid)
    {
        for (const auto& len: lengths)
        {
            std::string name = stringize(why.name, "_", len.name);
            bytestring buffer (regular);
            buffer[0] = std::uint8_t(len.length);
            buffer.resize(len.length + 1);
            why.invalidator(buffer);
            output.emplace_back(std::move(name), std::move(buffer), nullptr, type);
        }
    }
}

void add_time_samples(std::vector<binary_value_sample>& output)
{
    output.push_back(binary_value_sample("zero", {0x00},
        maket(0, 0, 0), protocol_field_type::time));
    output.push_back(binary_value_sample("positive_d",
        {0x08, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        maket(48,  0,  0, 0), protocol_field_type::time));
    output.push_back(binary_value_sample("positive_h",
        {0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00},
        maket(21, 0, 0, 0), protocol_field_type::time));
    output.push_back(binary_value_sample("positive_m",
        {0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x00},
        maket(0, 40, 0), protocol_field_type::time));
    output.push_back(binary_value_sample("positive_s",
        {0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x15},
        maket(0, 0, 21), protocol_field_type::time));
    output.push_back(binary_value_sample("positive_u",
        {0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe8, 0xe5, 0x04, 0x00},
        maket(0, 0, 0, 321000), protocol_field_type::time));
    output.push_back(binary_value_sample("positive_hmsu",
        {0x0c, 0x00, 0x22, 0x00, 0x00, 0x00, 0x16, 0x3b, 0x3a, 0x58, 0x3e, 0x0f, 0x00},
        maket(838, 59, 58, 999000), protocol_field_type::time));
    output.push_back(binary_value_sample("negative_d",
        {0x08, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        -maket(48,  0,  0, 0), protocol_field_type::time));
    output.push_back(binary_value_sample("negative_h",
        {0x08, 0x01, 0x00, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00},
        -maket(21, 0, 0, 0), protocol_field_type::time));
    output.push_back(binary_value_sample("negative_m",
        {0x08, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x00},
        -maket(0, 40, 0), protocol_field_type::time));
    output.push_back(binary_value_sample("negative_s",
        {0x08, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x15},
        -maket(0, 0, 21), protocol_field_type::time));
    output.push_back(binary_value_sample("negative_u",
        {0x0c, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe8, 0xe5, 0x04, 0x00},
        -maket(0, 0, 0, 321000), protocol_field_type::time));
    output.push_back(binary_value_sample("negative_hmsu",
        {0x0c, 0x01, 0x22, 0x00, 0x00, 0x00, 0x16, 0x3b, 0x3a, 0x58, 0x3e, 0x0f, 0x00},
        -maket(838, 59, 58, 999000), protocol_field_type::time));
    output.push_back(binary_value_sample("negative_sign_not_one",
        {0x0c, 0x03, 0x22, 0x00, 0x00, 0x00, 0x16, 0x3b, 0x3a, 0x58, 0x3e, 0x0f, 0x00},
        -maket(838, 59, 58, 999000), protocol_field_type::time));
}

std::vector<binary_value_sample> make_all_samples()
{
    std::vector<binary_value_sample> res;
    add_string_samples(res);
    add_int_samples(res);
    add_bit_types(res);
    add_float_samples(res);
    add_double_samples(res);
    add_date_samples(res);
    add_datetime_samples(protocol_field_type::datetime, res);
    add_datetime_samples(protocol_field_type::timestamp, res);
    add_time_samples(res);
    return res;
}

BOOST_DATA_TEST_CASE(test_deserialize_binary_value_ok, data::make(make_all_samples()))
{
    column_definition_packet coldef {};
    coldef.type = sample.type;
    coldef.flags = sample.flags;
    boost::mysql::field_metadata meta (coldef);
    value actual_value;
    const auto& buffer = sample.from;
    deserialization_context ctx (buffer.data(), buffer.data() + buffer.size(), capabilities());
    auto err = deserialize_binary_value(ctx, meta, actual_value);
    BOOST_TEST(err == errc::ok);
    BOOST_TEST(actual_value == sample.expected);
    BOOST_TEST(ctx.first() == buffer.data() + buffer.size()); // all bytes consumed
}


} // anon namespace
