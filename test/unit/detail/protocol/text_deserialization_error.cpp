//
// Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Test deserialize_text_value(), only error cases

#include <boost/mysql/detail/protocol/text_deserialization.hpp>
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

struct text_value_err_sample
{
    std::string name;
    boost::string_view from;
    protocol_field_type type;
    std::uint16_t flags;
    unsigned decimals;
    errc expected_err;

    text_value_err_sample(std::string&& name, boost::string_view from, protocol_field_type type,
            std::uint16_t flags=0, unsigned decimals=0, errc expected_err=errc::protocol_value_error) :
        name(std::move(name)),
        from(from),
        type(type),
        flags(flags),
        decimals(decimals),
        expected_err(expected_err)
    {
    }
};

std::ostream& operator<<(std::ostream& os, const text_value_err_sample& input)
{
    return os << "(input=" << input.from
              << ", type=" << to_string(input.type)
              << ", name=" << input.name
              << ")";
}

void add_int_samples(
    protocol_field_type t,
    std::vector<text_value_err_sample>& output
)
{
    output.emplace_back("signed_blank", "", t);
    output.emplace_back("signed_non_number", "abtrf", t);
    output.emplace_back("signed_hex", "0x01", t);
    output.emplace_back("signed_fractional", "1.1", t);
    output.emplace_back("signed_exp", "2e10", t);
    output.emplace_back("signed_lt_min", "-9223372036854775809", t);
    output.emplace_back("signed_gt_max", "9223372036854775808", t);
    output.emplace_back("unsigned_blank", "", t, column_flags::unsigned_);
    output.emplace_back("unsigned_non_number", "abtrf", t, column_flags::unsigned_);
    output.emplace_back("unsigned_hex", "0x01", t, column_flags::unsigned_);
    output.emplace_back("unsigned_fractional", "1.1", t, column_flags::unsigned_);
    output.emplace_back("unsigned_exp", "2e10", t, column_flags::unsigned_);
    output.emplace_back("unsigned_lt_min", "-18446744073709551616", t, column_flags::unsigned_);
    output.emplace_back("unsigned_gt_max", "18446744073709551616", t, column_flags::unsigned_);
}

void add_bit_samples(
    std::vector<text_value_err_sample>& output
)
{
    output.emplace_back("bit_string_view_too_short", "", protocol_field_type::bit, column_flags::unsigned_);
    output.emplace_back("bit_string_view_too_long", "123456789", protocol_field_type::bit, column_flags::unsigned_);
}

void add_float_samples(
    protocol_field_type t,
    boost::string_view lt_min,
    boost::string_view gt_max,
    std::vector<text_value_err_sample>& output
)
{
    output.emplace_back("blank", "", t);
    output.emplace_back("non_number", "abtrf", t);
    output.emplace_back("lt_min", lt_min, t);
    output.emplace_back("gt_max", gt_max, t);
    output.emplace_back("inf", "inf", t); // inf values not allowed by SQL std
    output.emplace_back("minus_inf", "-inf", t);
    output.emplace_back("nan", "nan", t); // nan values not allowed by SQL std
    output.emplace_back("minus_nan", "-nan", t);
}

void add_date_samples(std::vector<text_value_err_sample>& output)
{
    output.emplace_back("empty",            "", protocol_field_type::date);
    output.emplace_back("too_short",        "2020-05-2", protocol_field_type::date);
    output.emplace_back("too_long",         "02020-05-02", protocol_field_type::date);
    output.emplace_back("bad_delimiter",    "2020:05:02", protocol_field_type::date);
    output.emplace_back("too_many_groups",  "20-20-05-2", protocol_field_type::date);
    output.emplace_back("too_few_groups",   "2020-00005", protocol_field_type::date);
    output.emplace_back("incomplete_year",  "999-05-005", protocol_field_type::date);
    output.emplace_back("hex",              "ffff-ff-ff", protocol_field_type::date);
    output.emplace_back("null_value",       makesv("2020-05-\02"), protocol_field_type::date);
    output.emplace_back("long_year",     "10000-05-2", protocol_field_type::date);
    output.emplace_back("long_month",       "2010-005-2", protocol_field_type::date);
    output.emplace_back("long_day",         "2010-5-002", protocol_field_type::date);
    output.emplace_back("negative_year",    "-001-05-02", protocol_field_type::date);
    output.emplace_back("invalid_month",    "2010-13-02", protocol_field_type::date);
    output.emplace_back("invalid_month_max","2010-99-02", protocol_field_type::date);
    output.emplace_back("negative_month",   "2010--5-02", protocol_field_type::date);
    output.emplace_back("invalid_day",      "2010-05-32", protocol_field_type::date);
    output.emplace_back("invalid_day_max",  "2010-05-99", protocol_field_type::date);
    output.emplace_back("negative_day",     "2010-05--2", protocol_field_type::date);
}

void add_datetime_samples(
    protocol_field_type t,
    std::vector<text_value_err_sample>& output
)
{
    output.emplace_back("empty",            "", t);
    output.emplace_back("too_short_0",      "2020-05-02 23:01:0", t, 0, 0);
    output.emplace_back("too_short_1",      "2020-05-02 23:01:0.1", t, 0, 1);
    output.emplace_back("too_short_2",      "2020-05-02 23:01:00.1", t, 0, 2);
    output.emplace_back("too_short_3",      "2020-05-02 23:01:00.11", t, 0, 3);
    output.emplace_back("too_short_4",      "2020-05-02 23:01:00.111", t, 0, 4);
    output.emplace_back("too_short_5",      "2020-05-02 23:01:00.1111", t, 0, 5);
    output.emplace_back("too_short_6",      "2020-05-02 23:01:00.11111", t, 0, 6);
    output.emplace_back("too_long_0",       "2020-05-02 23:01:00.8", t, 0, 0);
    output.emplace_back("too_long_1",       "2020-05-02 23:01:00.98", t, 0, 1);
    output.emplace_back("too_long_2",       "2020-05-02 23:01:00.998", t, 0, 2);
    output.emplace_back("too_long_3",       "2020-05-02 23:01:00.9998", t, 0, 3);
    output.emplace_back("too_long_4",       "2020-05-02 23:01:00.99998", t, 0, 4);
    output.emplace_back("too_long_5",       "2020-05-02 23:01:00.999998", t, 0, 5);
    output.emplace_back("too_long_6",       "2020-05-02 23:01:00.9999998", t, 0, 6);
    output.emplace_back("no_decimals_1",    "2020-05-02 23:01:00  ", t, 0, 1);
    output.emplace_back("no_decimals_2",    "2020-05-02 23:01:00   ", t, 0, 2);
    output.emplace_back("no_decimals_3",    "2020-05-02 23:01:00     ", t, 0, 3);
    output.emplace_back("no_decimals_4",    "2020-05-02 23:01:00      ", t, 0, 4);
    output.emplace_back("no_decimals_5",    "2020-05-02 23:01:00       ", t, 0, 5);
    output.emplace_back("no_decimals_6",    "2020-05-02 23:01:00        ", t, 0, 6);
    output.emplace_back("trailing_0",       "2020-05-02 23:01:0p", t, 0, 0);
    output.emplace_back("trailing_1",       "2020-05-02 23:01:00.p", t, 0, 1);
    output.emplace_back("trailing_2",       "2020-05-02 23:01:00.1p", t, 0, 2);
    output.emplace_back("trailing_3",       "2020-05-02 23:01:00.12p", t, 0, 3);
    output.emplace_back("trailing_4",       "2020-05-02 23:01:00.123p", t, 0, 4);
    output.emplace_back("trailing_5",       "2020-05-02 23:01:00.1234p", t, 0, 5);
    output.emplace_back("trailing_6",       "2020-05-02 23:01:00.12345p", t, 0, 6);
    output.emplace_back("bad_delimiter",    "2020-05-02 23-01-00", t);
    output.emplace_back("missing_1gp_0",    "2020-05-02 23:01:  ", t);
    output.emplace_back("missing_2gp_0",    "2020-05-02 23:     ", t);
    output.emplace_back("missing_3gp_0",    "2020-05-02         ", t);
    output.emplace_back("missing_1gp_1",    "2020-05-02 23:01:.9  ", t);
    output.emplace_back("missing_2gp_1",    "2020-05-02 23:.9     ", t);
    output.emplace_back("missing_3gp_1",    "2020-05-02.9         ", t);
    output.emplace_back("invalid_year",     "10000-05-02 24:20:20.1", t, 0, 2);
    output.emplace_back("negative_year",    "-100-05-02 24:20:20", t);
    output.emplace_back("invalid_month",    "2020-13-02 24:20:20", t);
    output.emplace_back("negative_month",   "2020--5-02 24:20:20", t);
    output.emplace_back("invalid_day",      "2020-05-32 24:20:20", t);
    output.emplace_back("negative_day",     "2020-05--2 24:20:20", t);
    output.emplace_back("invalid_hour",     "2020-05-02 24:20:20", t);
    output.emplace_back("negative_hour",    "2020-05-02 -2:20:20", t);
    output.emplace_back("invalid_min",      "2020-05-02 22:60:20", t);
    output.emplace_back("negative_min",     "2020-05-02 22:-1:20", t);
    output.emplace_back("invalid_sec",      "2020-05-02 22:06:60", t);
    output.emplace_back("negative_sec",     "2020-05-02 22:06:-1", t);
    output.emplace_back("negative_micro_2", "2020-05-02 22:06:01.-1", t, 0, 2);
    output.emplace_back("negative_micro_3", "2020-05-02 22:06:01.-12", t, 0, 3);
    output.emplace_back("negative_micro_4", "2020-05-02 22:06:01.-123", t, 0, 4);
    output.emplace_back("negative_micro_5", "2020-05-02 22:06:01.-1234", t, 0, 5);
    output.emplace_back("negative_micro_6", "2020-05-02 22:06:01.-12345", t, 0, 6);
}

void add_time_samples(std::vector<text_value_err_sample>& output)
{
    output.emplace_back("empty",           "", protocol_field_type::time);
    output.emplace_back("not_numbers",     "abjkjdb67", protocol_field_type::time);
    output.emplace_back("too_short_0",     "1:20:20", protocol_field_type::time);
    output.emplace_back("too_short_1",     "1:20:20.1", protocol_field_type::time, 0, 1);
    output.emplace_back("too_short_2",     "01:20:20.1", protocol_field_type::time, 0, 2);
    output.emplace_back("too_short_3",     "01:20:20.12", protocol_field_type::time, 0, 3);
    output.emplace_back("too_short_4",     "01:20:20.123", protocol_field_type::time, 0, 4);
    output.emplace_back("too_short_5",     "01:20:20.1234", protocol_field_type::time, 0, 5);
    output.emplace_back("too_short_6",     "01:20:20.12345", protocol_field_type::time, 0, 6);
    output.emplace_back("too_long_0",      "-9999:40:40", protocol_field_type::time, 0, 0);
    output.emplace_back("too_long_1",      "-9999:40:40.1", protocol_field_type::time, 0, 1);
    output.emplace_back("too_long_2",      "-9999:40:40.12", protocol_field_type::time, 0, 2);
    output.emplace_back("too_long_3",      "-9999:40:40.123", protocol_field_type::time, 0, 3);
    output.emplace_back("too_long_4",      "-9999:40:40.1234", protocol_field_type::time, 0, 4);
    output.emplace_back("too_long_5",      "-9999:40:40.12345", protocol_field_type::time, 0, 5);
    output.emplace_back("too_long_6",      "-9999:40:40.123456", protocol_field_type::time, 0, 6);
    output.emplace_back("extra_long",      "-99999999:40:40.12345678", protocol_field_type::time, 0, 6);
    output.emplace_back("extra_long2",     "99999999999:40:40", protocol_field_type::time, 0, 6);
    output.emplace_back("decimals_0",      "01:20:20.1", protocol_field_type::time, 0, 0);
    output.emplace_back("no_decimals_1",   "01:20:20  ", protocol_field_type::time, 0, 1);
    output.emplace_back("no_decimals_2",   "01:20:20   ", protocol_field_type::time, 0, 2);
    output.emplace_back("no_decimals_3",   "01:20:20    ", protocol_field_type::time, 0, 3);
    output.emplace_back("no_decimals_4",   "01:20:20     ", protocol_field_type::time, 0, 4);
    output.emplace_back("no_decimals_5",   "01:20:20      ", protocol_field_type::time, 0, 5);
    output.emplace_back("no_decimals_6",   "01:20:20       ", protocol_field_type::time, 0, 6);
    output.emplace_back("bad_delimiter",   "01-20-20", protocol_field_type::time);
    output.emplace_back("missing_1gp_0",   "23:01:  ", protocol_field_type::time);
    output.emplace_back("missing_2gp_0",   "23:     ", protocol_field_type::time);
    output.emplace_back("missing_1gp_1",   "23:01:.9  ", protocol_field_type::time, 0, 1);
    output.emplace_back("missing_2gp_1",   "23:.9     ", protocol_field_type::time, 0, 1);
    output.emplace_back("invalid_min",     "22:60:20", protocol_field_type::time);
    output.emplace_back("negative_min",    "22:-1:20", protocol_field_type::time);
    output.emplace_back("invalid_sec",     "22:06:60", protocol_field_type::time);
    output.emplace_back("negative_sec",    "22:06:-1", protocol_field_type::time);
    output.emplace_back("invalid_micro_1", "22:06:01.99", protocol_field_type::time, 0, 1);
    output.emplace_back("invalid_micro_2", "22:06:01.999", protocol_field_type::time, 0, 2);
    output.emplace_back("invalid_micro_3", "22:06:01.9999", protocol_field_type::time, 0, 3);
    output.emplace_back("invalid_micro_4", "22:06:01.99999", protocol_field_type::time, 0, 4);
    output.emplace_back("invalid_micro_5", "22:06:01.999999", protocol_field_type::time, 0, 5);
    output.emplace_back("invalid_micro_6", "22:06:01.9999999", protocol_field_type::time, 0, 6);
    output.emplace_back("negative_micro",  "22:06:01.-1", protocol_field_type::time, 0, 2);
    output.emplace_back("lt_min",          "-900:00:00.00", protocol_field_type::time, 0, 2);
    output.emplace_back("gt_max",          "900:00:00.00", protocol_field_type::time, 0, 2);
    output.emplace_back("invalid_sign",    "x670:00:00.00", protocol_field_type::time, 0, 2);
    output.emplace_back("null_char",       makesv("20:00:\00.00"), protocol_field_type::time, 0, 2);
    output.emplace_back("trailing_0",      "22:06:01k", protocol_field_type::time, 0, 0);
    output.emplace_back("trailing_1",      "22:06:01.1k", protocol_field_type::time, 0, 1);
    output.emplace_back("trailing_2",      "22:06:01.12k", protocol_field_type::time, 0, 2);
    output.emplace_back("trailing_3",      "22:06:01.123k", protocol_field_type::time, 0, 3);
    output.emplace_back("trailing_4",      "22:06:01.1234k", protocol_field_type::time, 0, 4);
    output.emplace_back("trailing_5",      "22:06:01.12345k", protocol_field_type::time, 0, 5);
    output.emplace_back("trailing_6",      "22:06:01.123456k", protocol_field_type::time, 0, 6);
    output.emplace_back("double_sign",     "--22:06:01.123456", protocol_field_type::time, 0, 6);
}

std::vector<text_value_err_sample> make_all_samples()
{
    std::vector<text_value_err_sample> res;
    add_int_samples(protocol_field_type::tiny, res);
    add_int_samples(protocol_field_type::short_, res);
    add_int_samples(protocol_field_type::int24, res);
    add_int_samples(protocol_field_type::long_, res);
    add_int_samples(protocol_field_type::longlong, res);
    add_int_samples(protocol_field_type::year, res);
    add_bit_samples(res);
    add_float_samples(protocol_field_type::float_, "-2e90", "2e90", res);
    add_float_samples(protocol_field_type::double_, "-2e9999", "2e9999", res);
    add_date_samples(res);
    add_datetime_samples(protocol_field_type::datetime, res);
    add_datetime_samples(protocol_field_type::timestamp, res);
    add_time_samples(res);
    return res;
}

BOOST_DATA_TEST_CASE(test_deserialize_text_value_error, data::make(make_all_samples()))
{
    column_definition_packet coldef {};
    coldef.type = sample.type;
    coldef.decimals = static_cast<std::uint8_t>(sample.decimals);
    coldef.flags = sample.flags;
    boost::mysql::field_metadata meta (coldef);
    value actual_value;
    auto err = deserialize_text_value(sample.from, meta, actual_value);
    auto expected = sample.expected_err;
    BOOST_TEST(expected == err);
}

} // anon namespace
