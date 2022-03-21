//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_TEXT_DESERIALIZATION_IPP
#define BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_TEXT_DESERIALIZATION_IPP

#pragma once

#include <boost/mysql/detail/protocol/text_deserialization.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>
#include <boost/mysql/detail/protocol/date.hpp>
#include <boost/mysql/detail/protocol/bit_deserialization.hpp>
#include <cstdlib>
#include <cmath>
#include <type_traits>
#include <boost/config.hpp>
#include <boost/lexical_cast/try_lexical_convert.hpp>

#ifdef BOOST_MSVC
#pragma warning( push )
#pragma warning( disable : 4996 ) // MSVC doesn't like my sscanf's
#endif

namespace boost {
namespace mysql {
namespace detail {

// Integers
template <class T>
errc deserialize_text_value_int_impl(
    boost::string_view from,
    value& to
) noexcept
{
    T v;
    bool ok = boost::conversion::try_lexical_convert(from.data(), from.size(), v);
    if (!ok)
        return errc::protocol_value_error;
    to = value(v);
    return errc::ok;
}

inline errc deserialize_text_value_int(
    boost::string_view from,
    value& to,
    const field_metadata& meta
) noexcept
{
    return meta.is_unsigned() ?
            deserialize_text_value_int_impl<std::uint64_t>(from, to) :
            deserialize_text_value_int_impl<std::int64_t>(from, to);
}

// Floating points
template <class T>
errc deserialize_text_value_float(
    boost::string_view from,
    value& to
) noexcept
{
    T val;
    bool ok = boost::conversion::try_lexical_convert(from.data(), from.size(), val);
    if (!ok || std::isnan(val) || std::isinf(val)) // SQL std forbids these values
        return errc::protocol_value_error;
    to = value(val);
    return errc::ok;
}

// Strings
inline errc deserialize_text_value_string(
    boost::string_view from,
    value& to
) noexcept
{
    to = value(from);
    return errc::ok;
}

// Date/time types
inline unsigned sanitize_decimals(unsigned decimals) noexcept
{
    return std::min(decimals, textc::max_decimals);
}

// Computes the meaning of the parsed microsecond number, taking into
// account decimals (85 with 2 decimals means 850000us)
inline unsigned compute_micros(unsigned parsed_micros, unsigned decimals) noexcept
{
    return parsed_micros * static_cast<unsigned>(std::pow(10, textc::max_decimals - decimals));
}

inline errc deserialize_text_ymd(
    boost::string_view from,
    year_month_day& to
)
{
    using namespace textc;

    // Size check
    if (from.size() != date_sz)
        return errc::protocol_value_error;

    // Copy to a NULL-terminated buffer
    char buffer [date_sz + 1] {};
    std::memcpy(buffer, from.data(), from.size());

    // Parse individual components
    unsigned year, month, day;
    char extra_char;
    int parsed = sscanf(buffer, "%4u-%2u-%2u%c", &year, &month, &day, &extra_char);
    if (parsed != 3)
        return errc::protocol_value_error;

    // Range check for individual components
    if (year > max_year || month > max_month || day > max_day)
        return errc::protocol_value_error;

    to = year_month_day{static_cast<int>(year), month, day};
    return errc::ok;
}

inline errc deserialize_text_value_date(
    boost::string_view from,
    value& to
) noexcept
{
    // Deserialize ymd
    year_month_day ymd {};
    auto err = deserialize_text_ymd(from, ymd);
    if (err != errc::ok)
        return err;

    // Verify date validity. MySQL allows zero and invalid dates, which
    // we represent in C++ as NULL
    if (!is_valid(ymd))
    {
        to = value(nullptr);
        return errc::ok;
    }

    // Done
    to = value(date(days(ymd_to_days(ymd))));
    return errc::ok;
}

inline errc deserialize_text_value_datetime(
    boost::string_view from,
    value& to,
    const field_metadata& meta
) noexcept
{
    using namespace textc;

    // Sanitize decimals
    unsigned decimals = sanitize_decimals(meta.decimals());

    // Length check
    std::size_t expected_size = datetime_min_sz + (decimals ? decimals + 1 : 0);
    if (from.size() != expected_size)
        return errc::protocol_value_error;

    // Deserialize date part
    year_month_day ymd {};
    auto err = deserialize_text_ymd(from.substr(0, date_sz), ymd);
    if (err != errc::ok)
        return err;

    // Copy to NULL-terminated buffer
    constexpr std::size_t datetime_time_first = date_sz + 1; // date + space
    char buffer [datetime_max_sz - datetime_time_first + 1] {};
    std::memcpy(buffer, from.data() + datetime_time_first, from.size() - datetime_time_first);

    // Parse
    unsigned hours, minutes, seconds;
    unsigned micros = 0;
    char extra_char;
    if (decimals)
    {
        int parsed = sscanf(buffer, "%2u:%2u:%2u.%6u%c",
                &hours, &minutes, &seconds, &micros, &extra_char);
        if (parsed != 4)
            return errc::protocol_value_error;
        micros = compute_micros(micros, decimals);
    }
    else
    {
        int parsed = sscanf(buffer, "%2u:%2u:%2u%c",
                &hours, &minutes, &seconds, &extra_char);
        if (parsed != 3)
            return errc::protocol_value_error;
    }

    // Validity check. We make this check before
    // the invalid date check to make invalid dates with incorrect
    // hours/mins/secs/micros fail
    if (hours > max_hour ||
        minutes > max_min ||
        seconds > max_sec ||
        micros > max_micro)
    {
        return errc::protocol_value_error;
    }

    // Date validity. MySQL allows DATETIMEs with invalid dates, which
    // we represent here as NULL
    if (!is_valid(ymd))
    {
        to = value(nullptr);
        return errc::ok;
    }

    // Sum it up. Doing time of day independently to prevent overflow
    date d (days(ymd_to_days(ymd)));
    auto time_of_day =
        std::chrono::hours(hours) +
        std::chrono::minutes(minutes) +
        std::chrono::seconds(seconds) +
        std::chrono::microseconds(micros);
    to = value(d + time_of_day);
    return errc::ok;
}

inline errc deserialize_text_value_time(
    boost::string_view from,
    value& to,
    const field_metadata& meta
) noexcept
{
    using namespace textc;

    // Sanitize decimals
    unsigned decimals = sanitize_decimals(meta.decimals());

    // size check
    std::size_t actual_min_size = time_min_sz + (decimals ? decimals + 1 : 0);
    std::size_t actual_max_size = actual_min_size + 1 + 1; // hour extra character and sign
    assert(actual_max_size <= time_max_sz);
    if (from.size() < actual_min_size || from.size() > actual_max_size)
        return errc::protocol_value_error;

    // Copy to NULL-terminated buffer
    char buffer [time_max_sz + 1] {};
    memcpy(buffer, from.data(), from.size());

    // Sign
    bool is_negative = from[0] == '-';
    const char* first = is_negative ? buffer + 1 : buffer;

    // Parse it
    unsigned hours, minutes, seconds;
    unsigned micros = 0;
    char extra_char;
    if (decimals)
    {
        int parsed = sscanf(first, "%3u:%2u:%2u.%6u%c", &hours, &minutes, &seconds, &micros, &extra_char);
        if (parsed != 4)
            return errc::protocol_value_error;
        micros = compute_micros(micros, decimals);
    }
    else
    {
        int parsed = sscanf(first, "%3u:%2u:%2u%c", &hours, &minutes, &seconds, &extra_char);
        if (parsed != 3)
            return errc::protocol_value_error;
    }

    // Range check
    if (hours > time_max_hour ||
        minutes > max_min ||
        seconds > max_sec ||
        micros > max_micro)
    {
        return errc::protocol_value_error;
    }

    // Sum it
    auto res =
        std::chrono::hours(hours) +
        std::chrono::minutes(minutes) +
        std::chrono::seconds(seconds) +
        std::chrono::microseconds(micros);
    if (is_negative)
    {
        res = -res;
    }

    // Done
    to = value(res);
    return errc::ok;
}

inline bool is_next_field_null(
    const deserialization_context& ctx
)
{
    if (!ctx.enough_size(1))
        return false;
    return *ctx.first() == 0xfb;
}

} // detail
} // mysql
} // boost

inline boost::mysql::errc boost::mysql::detail::deserialize_text_value(
    boost::string_view from,
    const field_metadata& meta,
    value& output
)
{
    switch (meta.protocol_type())
    {
    case protocol_field_type::tiny:
    case protocol_field_type::short_:
    case protocol_field_type::int24:
    case protocol_field_type::long_:
    case protocol_field_type::year:
    case protocol_field_type::longlong:
        return deserialize_text_value_int(from, output, meta);
    case protocol_field_type::bit:
        return deserialize_bit(from, output);
    case protocol_field_type::float_:
        return deserialize_text_value_float<float>(from, output);
    case protocol_field_type::double_:
        return deserialize_text_value_float<double>(from, output);
    case protocol_field_type::timestamp:
    case protocol_field_type::datetime:
        return deserialize_text_value_datetime(from, output, meta);
    case protocol_field_type::date:
        return deserialize_text_value_date(from, output);
    case protocol_field_type::time:
        return deserialize_text_value_time(from, output, meta);
    // True string types
    case protocol_field_type::varchar:
    case protocol_field_type::var_string:
    case protocol_field_type::string:
    case protocol_field_type::tiny_blob:
    case protocol_field_type::medium_blob:
    case protocol_field_type::long_blob:
    case protocol_field_type::blob:
    case protocol_field_type::enum_:
    case protocol_field_type::set:
    // Anything else that we do not know how to interpret, we return as a binary string
    case protocol_field_type::decimal:
    case protocol_field_type::newdecimal:
    case protocol_field_type::geometry:
    default:
        return deserialize_text_value_string(from, output);
    }
}


boost::mysql::error_code boost::mysql::detail::deserialize_text_row(
    deserialization_context& ctx,
    const std::vector<field_metadata>& fields,
    std::vector<value>& output
)
{
    output.resize(fields.size());
    for (std::vector<value>::size_type i = 0; i < fields.size(); ++i)
    {
        if (is_next_field_null(ctx))
        {
            ctx.advance(1);
            output[i] = value(nullptr);
        }
        else
        {
            string_lenenc value_str;
            errc err = deserialize(ctx, value_str);
            if (err != errc::ok)
                return make_error_code(err);
            err = deserialize_text_value(value_str.value, fields[i], output[i]);
            if (err != errc::ok)
                return make_error_code(err);
        }
    }
    if (!ctx.empty())
        return make_error_code(errc::extra_bytes);
    return error_code();
}

#ifdef BOOST_MSVC
#pragma warning( pop )
#endif

#endif
