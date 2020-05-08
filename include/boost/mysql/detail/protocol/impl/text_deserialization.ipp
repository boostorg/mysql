//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_TEXT_DESERIALIZATION_IPP
#define BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_TEXT_DESERIALIZATION_IPP

#include <cstdlib>
#include <cmath>
#include <boost/lexical_cast/try_lexical_convert.hpp>
#include "boost/mysql/detail/protocol/constants.hpp"

namespace boost {
namespace mysql {
namespace detail {

inline unsigned sanitize_decimals(unsigned decimals)
{
    return std::min(decimals, textc::max_decimals);
}

// Computes the meaning of the parsed microsecond number, taking into
// account decimals (85 with 2 decimals means 850000us)
inline unsigned compute_micros(unsigned parsed_micros, unsigned decimals)
{
    return parsed_micros * static_cast<unsigned>(std::pow(10, textc::max_decimals - decimals));
}

inline errc deserialize_text_value_impl(
    std::string_view from,
    date& to
)
{
    // Size check
    if (from.size() != textc::date_sz)
        return errc::protocol_value_error;

    // Copy to a NULL-terminated buffer
    char buffer [textc::date_sz + 1] {};
    memcpy(buffer, from.data(), from.size());

    // Parse individual components
    unsigned year, month, day;
    int parsed = sscanf(buffer, "%4u-%2u-%2u", &year, &month, &day);
    if (parsed != 3)
        return errc::protocol_value_error;

    // Verify date validity
    ::date::year_month_day result (::date::year(year)/::date::month(month)/::date::day(day));
    if (!result.ok())
        return errc::protocol_value_error;

    // Range check
    to = result;
    if (to < min_date || to > max_date)
        return errc::protocol_value_error;

    return errc::ok;
}

inline errc deserialize_text_value_impl(
    std::string_view from,
    time& to,
    unsigned decimals
)
{
    using namespace textc;

    // Adjust decimals
    decimals = sanitize_decimals(decimals);

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
    if (hours > max_hour || minutes > max_min || seconds > max_sec || micros > max_micro)
        return errc::protocol_value_error;

    // Sum it
    auto res = std::chrono::hours(hours) + std::chrono::minutes(minutes) +
               std::chrono::seconds(seconds) + std::chrono::microseconds(micros);
    if (is_negative)
    {
        res = -res;
    }

    // Done
    to = res;
    return errc::ok;
}

inline errc deserialize_text_value_impl(
    std::string_view from,
    datetime& to,
    unsigned decimals
)
{
    using namespace textc;

    // Sanitize decimals
    decimals = sanitize_decimals(decimals);

    // Length check
    std::size_t expected_size = datetime_min_sz + (decimals ? decimals + 1 : 0);
    if (from.size() != expected_size)
        return errc::protocol_value_error;

    // Parse date
    date d;
    auto err = deserialize_text_value_impl(from.substr(0, date_sz), d);
    if (err != errc::ok)
        return err;

    // Time of day part
    time time_of_day;
    err = deserialize_text_value_impl(from.substr(date_sz + 1), time_of_day, decimals);
    if (err != errc::ok)
        return err;

    // Range check
    constexpr auto max_time_of_day = std::chrono::hours(24) - std::chrono::microseconds(1);
    if (time_of_day < std::chrono::seconds(0) || time_of_day > max_time_of_day)
        return errc::protocol_value_error;

    // Sum it up
    to = d + time_of_day;
    return errc::ok;
}


template <typename T>
std::enable_if_t<std::is_arithmetic_v<T>, errc>
deserialize_text_value_impl(std::string_view from, T& to)
{
    bool ok = boost::conversion::try_lexical_convert(from.data(), from.size(), to);
    if constexpr (std::is_floating_point_v<T>)
    {
        ok &= !(std::isnan(to) || std::isinf(to)); // SQL std forbids these values
    }
    return ok ? errc::ok : errc::protocol_value_error;
}

inline errc deserialize_text_value_impl(std::string_view from, std::string_view& to)
{
    to = from;
    return errc::ok;
}

template <typename T, typename... Args>
errc deserialize_text_value_to_variant(std::string_view from, value& to, Args&&... args)
{
    return deserialize_text_value_impl(from, to.emplace<T>(), std::forward<Args>(args)...);
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
    std::string_view from,
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
        return meta.is_unsigned() ?
                deserialize_text_value_to_variant<std::uint32_t>(from, output) :
                deserialize_text_value_to_variant<std::int32_t>(from, output);
    case protocol_field_type::longlong:
        return meta.is_unsigned() ?
                deserialize_text_value_to_variant<std::uint64_t>(from, output) :
                deserialize_text_value_to_variant<std::int64_t>(from, output);
    case protocol_field_type::float_:
        return deserialize_text_value_to_variant<float>(from, output);
    case protocol_field_type::double_:
        return deserialize_text_value_to_variant<double>(from, output);
    case protocol_field_type::timestamp:
    case protocol_field_type::datetime:
        return deserialize_text_value_to_variant<datetime>(from, output, meta.decimals());
    case protocol_field_type::date:
        return deserialize_text_value_to_variant<date>(from, output);
    case protocol_field_type::time:
        return deserialize_text_value_to_variant<time>(from, output, meta.decimals());
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
    case protocol_field_type::bit:
    case protocol_field_type::newdecimal:
    case protocol_field_type::geometry:
    default:
        return deserialize_text_value_to_variant<std::string_view>(from, output);
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
            output[i] = nullptr;
        }
        else
        {
            string_lenenc value_str;
            errc err = deserialize(value_str, ctx);
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



#endif
