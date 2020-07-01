//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Test deserialize_text_value(), just positive cases

#include "boost/mysql/detail/protocol/text_deserialization.hpp"
#include "test_common.hpp"
#include <boost/test/data/monomorphic/collection.hpp>
#include <boost/test/data/test_case.hpp>

using namespace boost::mysql::detail;
using namespace boost::mysql::test;
using namespace boost::unit_test;
using boost::mysql::value;
using boost::mysql::error_code;
using boost::mysql::errc;

namespace
{

struct text_value_sample
{
    std::string name;
    std::string from;
    value expected;
    protocol_field_type type;
    unsigned decimals;
    std::uint16_t flags;

    template <class T>
    text_value_sample(
        std::string&& name,
        std::string&& from,
        T&& expected_value,
        protocol_field_type type,
        std::uint16_t flags=0,
        unsigned decimals=0
    ) :
        name(std::move(name)),
        from(std::move(from)),
        expected(std::forward<T>(expected_value)),
        type(type),
        decimals(decimals),
        flags(flags)
    {
    }
};

std::ostream& operator<<(std::ostream& os, const text_value_sample& input)
{
    return os << "(input=" << input.from
              << ", type=" << to_string(input.type)
              << ", name=" << input.name
              << ")";
}

void add_string_samples(std::vector<text_value_sample>& output)
{
    output.emplace_back("varchar_non_empty", "string", "string", protocol_field_type::var_string);
    output.emplace_back("varchar_empty", "", "", protocol_field_type::var_string);
    output.emplace_back("char", "", "", protocol_field_type::string);
    output.emplace_back("varbinary", "value", "value", protocol_field_type::var_string, column_flags::binary);
    output.emplace_back("binary", "value", "value", protocol_field_type::string, column_flags::binary);
    output.emplace_back("text_blob", "value", "value", protocol_field_type::blob, column_flags::blob);
    output.emplace_back("enum", "value", "value", protocol_field_type::string, column_flags::enum_);
    output.emplace_back("set", "value1,value2", "value1,value2", protocol_field_type::string, column_flags::set);

    output.emplace_back("bit", "\1", "\1", protocol_field_type::bit);
    output.emplace_back("decimal", "\1", "\1", protocol_field_type::newdecimal);
    output.emplace_back("geometry", "\1", "\1", protocol_field_type::geometry,
            column_flags::binary | column_flags::blob);

    // Anything we don't know what it is, we interpret as a string
    output.emplace_back("unknown_protocol_type", "test",
            "test", static_cast<protocol_field_type>(0x23));
}

void add_int_samples_helper(
    std::string signed_max_s,
    std::int64_t signed_max_b,
    std::string signed_min_s,
    std::int64_t signed_min_b,
    std::string unsigned_max_s,
    std::uint64_t unsigned_max_b,
    std::string zerofill_s,
    std::uint64_t zerofill_b,
    protocol_field_type type,
    std::vector<text_value_sample>& output
)
{
    output.emplace_back("signed", "20", std::int64_t(20), type);
    output.emplace_back("signed_max", std::move(signed_max_s), signed_max_b, type);
    output.emplace_back("signed_negative", "-20", std::int64_t(-20), type);
    output.emplace_back("signed_min", std::move(signed_min_s), signed_min_b, type);
    output.emplace_back("unsigned", "20", std::uint64_t(20), type, column_flags::unsigned_);
    output.emplace_back("unsigned_min", "0", std::uint64_t(0), type, column_flags::unsigned_);
    output.emplace_back("unsigned_max", std::move(unsigned_max_s),
        unsigned_max_b, type, column_flags::unsigned_);
    output.emplace_back("unsigned_zerofill", std::move(zerofill_s),
        zerofill_b, type, column_flags::unsigned_ | column_flags::zerofill);
}

void add_int_samples(std::vector<text_value_sample>& output)
{
    add_int_samples_helper(
        "127", 127,
        "-128", -128,
        "255", 255,
        "010", 10,
        protocol_field_type::tiny,
        output
    );
    add_int_samples_helper(
        "32767", 32767,
        "-32768", -32768,
        "65535", 65535,
        "00535", 535,
        protocol_field_type::short_,
        output
    );
    add_int_samples_helper(
        "8388607", 8388607,
        "-8388608",-8388608,
        "16777215", 16777215,
        "00007215", 7215,
        protocol_field_type::int24,
        output
    );
    add_int_samples_helper(
        "2147483647", 2147483647,
        "-2147483648", -2147483647 - 1, // minus is not part of literal, avoids warning
        "4294967295", 4294967295,
        "0000067295", 67295,
        protocol_field_type::long_,
        output
    );
    add_int_samples_helper(
        "9223372036854775807", 9223372036854775807,
        "-9223372036854775808", -9223372036854775807 - 1, // minus is not part of literal, avoids warning
        "18446744073709551615", 18446744073709551615ULL, // suffix needed to avoid warning
        "0000067295", 67295,
        protocol_field_type::longlong,
        output
    );
    add_int_samples_helper(
        "9223372036854775807", 9223372036854775807,
        "-9223372036854775808", -9223372036854775807 - 1, // minus is not part of literal, avoids warning
        "18446744073709551615", 18446744073709551615ULL, // suffix needed to avoid warning
        "0000067295", 67295,
        protocol_field_type::longlong,
        output
    );

    // YEAR
    output.emplace_back("regular_value", "1999", std::uint64_t(1999), protocol_field_type::year, column_flags::unsigned_);
    output.emplace_back("min", "1901", std::uint64_t(1901), protocol_field_type::year, column_flags::unsigned_);
    output.emplace_back("max", "2155", std::uint64_t(2155), protocol_field_type::year, column_flags::unsigned_);
    output.emplace_back("zero", "0000", std::uint64_t(0), protocol_field_type::year, column_flags::unsigned_);
}

template <class T>
void add_float_samples(
    protocol_field_type type,
    std::vector<text_value_sample>& output
)
{
    output.emplace_back("zero", "0", T(0.0),  type);
    output.emplace_back("integer_positive", "4", T(4.0),  type);
    output.emplace_back("integer_negative", "-5", T(-5.0),  type);
    output.emplace_back("fractional_positive", "3.147", T(3.147),  type);
    output.emplace_back("fractional_negative", "-3.147", T(-3.147),  type);
    output.emplace_back("positive_exponent_positive_integer", "3e20", T(3e20),  type);
    output.emplace_back("positive_exponent_negative_integer", "-3e20", T(-3e20),  type);
    output.emplace_back("positive_exponent_positive_fractional", "3.14e20", T(3.14e20),  type);
    output.emplace_back("positive_exponent_negative_fractional", "-3.45e20", T(-3.45e20),  type);
    output.emplace_back("negative_exponent_positive_integer", "3e-20", T(3e-20),  type);
    output.emplace_back("negative_exponent_negative_integer", "-3e-20", T(-3e-20),  type);
    output.emplace_back("negative_exponent_positive_fractional", "3.14e-20", T(3.14e-20),  type);
    output.emplace_back("negative_exponent_negative_fractional", "-3.45e-20", T(-3.45e-20),  type);
}

void add_date_samples(std::vector<text_value_sample>& output)
{
    output.emplace_back("regular_date", "2019-02-28", makedate(2019, 2, 28), protocol_field_type::date);
    output.emplace_back("leap_year", "1788-02-29", makedate(1788, 2, 29), protocol_field_type::date);
    output.emplace_back("min", "0000-01-01", makedate(0, 1, 1), protocol_field_type::date);
    output.emplace_back("max", "9999-12-31", makedate(9999, 12, 31), protocol_field_type::date);
    output.emplace_back("zero", "0000-00-00", nullptr, protocol_field_type::date);
    output.emplace_back("zero_month", "0000-00-01", nullptr, protocol_field_type::date);
    output.emplace_back("zero_day", "0000-01-00", nullptr, protocol_field_type::date);
    output.emplace_back("zero_month_day_nonzero_year", "2010-00-00", nullptr, protocol_field_type::date);
    output.emplace_back("invalid_date", "2010-11-31", nullptr, protocol_field_type::date);
}

void add_datetime_samples(
    protocol_field_type type,
    std::vector<text_value_sample>& output
)
{
    output.emplace_back("0_decimals_date", "2010-02-15 00:00:00", makedt(2010, 2, 15), type);
    output.emplace_back("0_decimals_h", "2010-02-15 02:00:00", makedt(2010, 2, 15, 2), type);
    output.emplace_back("0_decimals_hm", "2010-02-15 02:05:00", makedt(2010, 2, 15, 2, 5), type);
    output.emplace_back("0_decimals_hms", "2010-02-15 02:05:30", makedt(2010, 2, 15, 2, 5, 30), type);
    output.emplace_back("0_decimals_min", "0000-01-01 00:00:00", makedt(0, 1, 1), type);
    output.emplace_back("0_decimals_max", "9999-12-31 23:59:59", makedt(9999, 12, 31, 23, 59, 59), type);

    output.emplace_back("1_decimals_date", "2010-02-15 00:00:00.0", makedt(2010, 2, 15), type, 0, 1);
    output.emplace_back("1_decimals_h", "2010-02-15 02:00:00.0", makedt(2010, 2, 15, 2), type, 0, 1);
    output.emplace_back("1_decimals_hm", "2010-02-15 02:05:00.0", makedt(2010, 2, 15, 2, 5), type, 0, 1);
    output.emplace_back("1_decimals_hms", "2010-02-15 02:05:30.0", makedt(2010, 2, 15, 2, 5, 30), type, 0, 1);
    output.emplace_back("1_decimals_hmsu", "2010-02-15 02:05:30.5", makedt(2010, 2, 15, 2, 5, 30, 500000), type, 0, 1);
    output.emplace_back("1_decimals_min", "0000-01-01 00:00:00.0", makedt(0, 1, 1), type, 0, 1);
    output.emplace_back("1_decimals_max", "9999-12-31 23:59:59.9", makedt(9999, 12, 31, 23, 59, 59, 900000), type, 0, 1);

    output.emplace_back("2_decimals_hms", "2010-02-15 02:05:30.00", makedt(2010, 2, 15, 2, 5, 30), type, 0, 2);
    output.emplace_back("2_decimals_hmsu", "2010-02-15 02:05:30.05", makedt(2010, 2, 15, 2, 5, 30, 50000), type, 0, 2);
    output.emplace_back("2_decimals_min", "0000-01-01 00:00:00.00", makedt(0, 1, 1), type, 0, 2);
    output.emplace_back("2_decimals_max", "9999-12-31 23:59:59.99", makedt(9999, 12, 31, 23, 59, 59, 990000), type, 0, 2);

    output.emplace_back("3_decimals_hms", "2010-02-15 02:05:30.000", makedt(2010, 2, 15, 2, 5, 30), type, 0, 3);
    output.emplace_back("3_decimals_hmsu", "2010-02-15 02:05:30.420", makedt(2010, 2, 15, 2, 5, 30, 420000), type, 0, 3);
    output.emplace_back("3_decimals_min", "0000-01-01 00:00:00.000", makedt(0, 1, 1), type, 0, 3);
    output.emplace_back("3_decimals_max", "9999-12-31 23:59:59.999", makedt(9999, 12, 31, 23, 59, 59, 999000), type, 0, 3);

    output.emplace_back("4_decimals_hms", "2010-02-15 02:05:30.0000", makedt(2010, 2, 15, 2, 5, 30), type, 0, 4);
    output.emplace_back("4_decimals_hmsu", "2010-02-15 02:05:30.4267", makedt(2010, 2, 15, 2, 5, 30, 426700), type, 0, 4);
    output.emplace_back("4_decimals_min", "0000-01-01 00:00:00.0000", makedt(0, 1, 1), type, 0, 4);
    output.emplace_back("4_decimals_max", "9999-12-31 23:59:59.9999", makedt(9999, 12, 31, 23, 59, 59, 999900), type, 0, 4);

    output.emplace_back("5_decimals_hms", "2010-02-15 02:05:30.00000", makedt(2010, 2, 15, 2, 5, 30), type, 0, 5);
    output.emplace_back("5_decimals_hmsu", "2010-02-15 02:05:30.00239", makedt(2010, 2, 15, 2, 5, 30, 2390), type, 0, 5);
    output.emplace_back("5_decimals_min", "0000-01-01 00:00:00.00000", makedt(0, 1, 1), type, 0, 5);
    output.emplace_back("5_decimals_max", "9999-12-31 23:59:59.99999", makedt(9999, 12, 31, 23, 59, 59, 999990), type, 0, 5);

    output.emplace_back("6_decimals_hms", "2010-02-15 02:05:30.000000", makedt(2010, 2, 15, 2, 5, 30), type, 0, 6);
    output.emplace_back("6_decimals_hmsu", "2010-02-15 02:05:30.002395", makedt(2010, 2, 15, 2, 5, 30, 2395), type, 0, 6);
    output.emplace_back("6_decimals_min", "0000-01-01 00:00:00.000000", makedt(0, 1, 1), type, 0, 6);
    output.emplace_back("6_decimals_max", "9999-12-31 23:59:59.999999", makedt(9999, 12, 31, 23, 59, 59, 999999), type, 0, 6);

    // not a real case, we cap decimals to 6
    output.emplace_back("7_decimals", "2010-02-15 02:05:30.002395", makedt(2010, 2, 15, 2, 5, 30, 2395), type, 0, 7);

    // Generate all invalid date casuistic for all decimals
    constexpr struct
    {
        const char* name;
        const char* base_value;
    } why_is_invalid [] = {
        { "zero", "0000-00-00 00:00:00" },
        { "invalid_date", "2010-11-31 01:10:59" },
        { "zero_month",  "2010-00-31 01:10:59" },
        { "zero_day", "2010-11-00 01:10:59" },
        { "zero_month_day", "2010-00-00 01:10:59" }
    };

    for (const auto& why: why_is_invalid)
    {
        for (unsigned decimals = 0; decimals <= 6; ++decimals)
        {
            std::string name = stringize(decimals, "_decimals_", why.name);
            std::string value = why.base_value;
            if (decimals > 0)
            {
                value.push_back('.');
                for (unsigned i = 0; i < decimals; ++i)
                    value.push_back('0');
            }
            output.emplace_back(std::move(name), std::move(value), nullptr, type, 0, decimals);
        }
    }
}

void add_time_samples(std::vector<text_value_sample>& output)
{
    output.emplace_back("0_decimals_positive_h", "01:00:00", maket(1, 0, 0), protocol_field_type::time);
    output.emplace_back("0_decimals_positive_hm", "12:03:00", maket(12, 3, 0), protocol_field_type::time);
    output.emplace_back("0_decimals_positive_hms", "14:51:23", maket(14, 51, 23), protocol_field_type::time);
    output.emplace_back("0_decimals_max", "838:59:59", maket(838, 59, 59), protocol_field_type::time);
    output.emplace_back("0_decimals_negative_h", "-06:00:00", -maket(6, 0, 0), protocol_field_type::time);
    output.emplace_back("0_decimals_negative_hm", "-12:03:00", -maket(12, 3, 0), protocol_field_type::time);
    output.emplace_back("0_decimals_negative_hms", "-14:51:23", -maket(14, 51, 23), protocol_field_type::time);
    output.emplace_back("0_decimals_min", "-838:59:59", -maket(838, 59, 59), protocol_field_type::time);
    output.emplace_back("0_decimals_zero", "00:00:00", maket(0, 0, 0), protocol_field_type::time);
    output.emplace_back("0_decimals_negative_h0", "-00:51:23", -maket(0, 51, 23), protocol_field_type::time);

    output.emplace_back("1_decimals_positive_hms", "14:51:23.0", maket(14, 51, 23), protocol_field_type::time, 0, 1);
    output.emplace_back("1_decimals_positive_hmsu", "14:51:23.5", maket(14, 51, 23, 500000), protocol_field_type::time, 0, 1);
    output.emplace_back("1_decimals_max", "838:59:58.9", maket(838, 59, 58, 900000), protocol_field_type::time, 0, 1);
    output.emplace_back("1_decimals_negative_hms", "-14:51:23.0", -maket(14, 51, 23), protocol_field_type::time, 0, 1);
    output.emplace_back("1_decimals_negative_hmsu", "-14:51:23.5", -maket(14, 51, 23, 500000), protocol_field_type::time, 0, 1);
    output.emplace_back("1_decimals_min", "-838:59:58.9", -maket(838, 59, 58, 900000), protocol_field_type::time, 0, 1);
    output.emplace_back("1_decimals_zero", "00:00:00.0", maket(0, 0, 0), protocol_field_type::time, 0, 1);
    output.emplace_back("1_decimals_negative_h0", "-00:51:23.1", -maket(0, 51, 23, 100000), protocol_field_type::time, 0, 1);

    output.emplace_back("2_decimals_positive_hms", "14:51:23.00", maket(14, 51, 23), protocol_field_type::time, 0, 2);
    output.emplace_back("2_decimals_positive_hmsu", "14:51:23.52", maket(14, 51, 23, 520000), protocol_field_type::time, 0, 2);
    output.emplace_back("2_decimals_max", "838:59:58.99", maket(838, 59, 58, 990000), protocol_field_type::time, 0, 2);
    output.emplace_back("2_decimals_negative_hms", "-14:51:23.00", -maket(14, 51, 23), protocol_field_type::time, 0, 2);
    output.emplace_back("2_decimals_negative_hmsu", "-14:51:23.50", -maket(14, 51, 23, 500000), protocol_field_type::time, 0, 2);
    output.emplace_back("2_decimals_min", "-838:59:58.99", -maket(838, 59, 58, 990000), protocol_field_type::time, 0, 2);
    output.emplace_back("2_decimals_zero", "00:00:00.00", maket(0, 0, 0), protocol_field_type::time, 0, 2);
    output.emplace_back("2_decimals_negative_h0", "-00:51:23.12", -maket(0, 51, 23, 120000), protocol_field_type::time, 0, 2);

    output.emplace_back("3_decimals_positive_hms", "14:51:23.000", maket(14, 51, 23), protocol_field_type::time, 0, 3);
    output.emplace_back("3_decimals_positive_hmsu", "14:51:23.501", maket(14, 51, 23, 501000), protocol_field_type::time, 0, 3);
    output.emplace_back("3_decimals_max", "838:59:58.999", maket(838, 59, 58, 999000), protocol_field_type::time, 0, 3);
    output.emplace_back("3_decimals_negative_hms", "-14:51:23.000", -maket(14, 51, 23), protocol_field_type::time, 0, 3);
    output.emplace_back("3_decimals_negative_hmsu", "-14:51:23.003", -maket(14, 51, 23, 3000), protocol_field_type::time, 0, 3);
    output.emplace_back("3_decimals_min", "-838:59:58.999", -maket(838, 59, 58, 999000), protocol_field_type::time, 0, 3);
    output.emplace_back("3_decimals_zero", "00:00:00.000", maket(0, 0, 0), protocol_field_type::time, 0, 3);
    output.emplace_back("3_decimals_negative_h0", "-00:51:23.123", -maket(0, 51, 23, 123000), protocol_field_type::time, 0, 3);

    output.emplace_back("4_decimals_positive_hms", "14:51:23.0000", maket(14, 51, 23), protocol_field_type::time, 0, 4);
    output.emplace_back("4_decimals_positive_hmsu", "14:51:23.5017", maket(14, 51, 23, 501700), protocol_field_type::time, 0, 4);
    output.emplace_back("4_decimals_max", "838:59:58.9999", maket(838, 59, 58, 999900), protocol_field_type::time, 0, 4);
    output.emplace_back("4_decimals_negative_hms", "-14:51:23.0000", -maket(14, 51, 23), protocol_field_type::time, 0, 4);
    output.emplace_back("4_decimals_negative_hmsu", "-14:51:23.0038", -maket(14, 51, 23, 3800), protocol_field_type::time, 0, 4);
    output.emplace_back("4_decimals_min", "-838:59:58.9999", -maket(838, 59, 58, 999900), protocol_field_type::time, 0, 4);
    output.emplace_back("4_decimals_zero", "00:00:00.0000", maket(0, 0, 0), protocol_field_type::time, 0, 4);
    output.emplace_back("4_decimals_negative_h0", "-00:51:23.1234", -maket(0, 51, 23, 123400), protocol_field_type::time, 0, 4);

    output.emplace_back("5_decimals_positive_hms", "14:51:23.00000", maket(14, 51, 23), protocol_field_type::time, 0, 5);
    output.emplace_back("5_decimals_positive_hmsu", "14:51:23.50171", maket(14, 51, 23, 501710), protocol_field_type::time, 0, 5);
    output.emplace_back("5_decimals_max", "838:59:58.99999", maket(838, 59, 58, 999990), protocol_field_type::time, 0, 5);
    output.emplace_back("5_decimals_negative_hms", "-14:51:23.00000", -maket(14, 51, 23), protocol_field_type::time, 0, 5);
    output.emplace_back("5_decimals_negative_hmsu", "-14:51:23.00009", -maket(14, 51, 23, 90), protocol_field_type::time, 0, 5);
    output.emplace_back("5_decimals_min", "-838:59:58.99999", -maket(838, 59, 58, 999990), protocol_field_type::time, 0, 5);
    output.emplace_back("5_decimals_zero", "00:00:00.00000", maket(0, 0, 0), protocol_field_type::time, 0, 5);
    output.emplace_back("5_decimals_negative_h0", "-00:51:23.12345", -maket(0, 51, 23, 123450), protocol_field_type::time, 0, 5);

    output.emplace_back("6_decimals_positive_hms", "14:51:23.000000", maket(14, 51, 23), protocol_field_type::time, 0, 6);
    output.emplace_back("6_decimals_positive_hmsu", "14:51:23.501717", maket(14, 51, 23, 501717), protocol_field_type::time, 0, 6);
    output.emplace_back("6_decimals_max", "838:59:58.999999", maket(838, 59, 58, 999999), protocol_field_type::time, 0, 6);
    output.emplace_back("6_decimals_negative_hms", "-14:51:23.000000", -maket(14, 51, 23), protocol_field_type::time, 0, 6);
    output.emplace_back("6_decimals_negative_hmsu", "-14:51:23.900000", -maket(14, 51, 23, 900000), protocol_field_type::time, 0, 6);
    output.emplace_back("6_decimals_min", "-838:59:58.999999", -maket(838, 59, 58, 999999), protocol_field_type::time, 0, 6);
    output.emplace_back("6_decimals_zero", "00:00:00.000000", maket(0, 0, 0), protocol_field_type::time, 0, 6);
    output.emplace_back("6_decimals_negative_h0", "-00:51:23.123456", -maket(0, 51, 23, 123456), protocol_field_type::time, 0, 6);

    // This is not a real case - we cap anything above 6 decimals to 6
    output.emplace_back("7_decimals", "14:51:23.501717", maket(14, 51, 23, 501717), protocol_field_type::time, 0, 7);
}

std::vector<text_value_sample> make_all_samples()
{
    std::vector<text_value_sample> res;
    add_string_samples(res);
    add_int_samples(res);
    add_float_samples<float>(protocol_field_type::float_, res);
    add_float_samples<double>(protocol_field_type::double_, res);
    add_date_samples(res);
    add_datetime_samples(protocol_field_type::datetime, res);
    add_datetime_samples(protocol_field_type::timestamp, res);
    add_time_samples(res);
    return res;
}

BOOST_DATA_TEST_CASE(test_deserialize_text_value_ok, data::make(make_all_samples()))
{
    column_definition_packet coldef {};
    coldef.type = sample.type;
    coldef.decimals = static_cast<std::uint8_t>(sample.decimals);
    coldef.flags = sample.flags;
    boost::mysql::field_metadata meta (coldef);
    value actual_value;
    auto err = deserialize_text_value(sample.from, meta, actual_value);
    BOOST_TEST(err == errc::ok);
    BOOST_TEST(actual_value == sample.expected);
}

} // anon namespace
