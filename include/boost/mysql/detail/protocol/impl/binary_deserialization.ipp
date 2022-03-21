//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_BINARY_DESERIALIZATION_IPP
#define BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_BINARY_DESERIALIZATION_IPP

#pragma once

#include <boost/mysql/detail/protocol/binary_deserialization.hpp>
#include <boost/mysql/detail/protocol/serialization.hpp>
#include <boost/mysql/detail/protocol/null_bitmap_traits.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>
#include <boost/mysql/detail/protocol/date.hpp>
#include <boost/mysql/detail/protocol/bit_deserialization.hpp>

namespace boost {
namespace mysql {
namespace detail {

// strings
inline errc deserialize_binary_value_string(
    deserialization_context& ctx,
    value& output
) noexcept
{
    string_lenenc deser;
    auto err = deserialize(ctx, deser);
    if (err != errc::ok)
        return err;
    output = value(deser.value);
    return errc::ok;
}

// ints
template <class TargetType, class DeserializableType>
errc deserialize_binary_value_int_impl(
    deserialization_context& ctx,
    value& output
) noexcept
{
    DeserializableType deser;
    auto err = deserialize(ctx, deser);
    if (err != errc::ok)
        return err;
    output = value(static_cast<TargetType>(deser));
    return errc::ok;
}

template <
    class DeserializableTypeUnsigned,
    class DeserializableTypeSigned
>
errc deserialize_binary_value_int(
    const field_metadata& meta,
    deserialization_context& ctx,
    value& output
) noexcept
{
    return meta.is_unsigned() ?
        deserialize_binary_value_int_impl<
            std::uint64_t, DeserializableTypeUnsigned>(ctx, output) :
        deserialize_binary_value_int_impl<
            std::int64_t, DeserializableTypeSigned>(ctx, output);
}

// Bits. These come as a binary value between 1 and 8 bytes,
// packed in a string
inline errc deserialize_binary_value_bit(
    deserialization_context& ctx,
    value& output
) noexcept
{
    string_lenenc buffer;
    auto err = deserialize(ctx, buffer);
    if (err != errc::ok) return err;
    return deserialize_bit(buffer.value, output);
}

// Floats
template <class T>
errc deserialize_binary_value_float(
    deserialization_context& ctx,
    value& output
) noexcept
{
    // Size check
    if (!ctx.enough_size(sizeof(T)))
        return errc::incomplete_message;

    // Endianness conversion. Boost.Endian support for floats start at 1.71
    T v = boost::endian::endian_load<T, sizeof(T),
        boost::endian::order::little>(ctx.first());

    // Nans and infs not allowed in SQL
    if (std::isnan(v) || std::isinf(v))
        return errc::protocol_value_error;

    // Done
    ctx.advance(sizeof(T));
    output = value(v);
    return errc::ok;
}

// Time types
inline errc deserialize_binary_ymd(
    deserialization_context& ctx,
    year_month_day& output
)
{
    std::uint16_t year;
    std::uint8_t month;
    std::uint8_t day;

    // Deserialize
    auto err = deserialize(ctx, year, month, day);
    if (err != errc::ok)
        return err;

    // Range check
    if (year > max_year ||
        month > max_month ||
        day > max_day)
    {
        return errc::protocol_value_error;
    }

    output = year_month_day {year, month, day};

    return errc::ok;
}

inline errc deserialize_binary_value_date(
    deserialization_context& ctx,
    value& output
) noexcept
{
    // Deserialize length
    std::uint8_t length;
    auto err = deserialize(ctx, length);
    if (err != errc::ok)
        return err;

    // Check for zero dates, represented in C++ as a NULL
    if (length < binc::date_sz)
    {
        output = value(nullptr);
        return errc::ok;
    }

    // Deserialize rest of fields
    year_month_day ymd;
    err = deserialize_binary_ymd(ctx, ymd);
    if (err != errc::ok)
        return err;

    // Check for invalid dates, represented as NULL in C++
    if (!is_valid(ymd))
    {
        output = value(nullptr);
        return errc::ok;
    }

    // Convert to value
    output = value(date(days(ymd_to_days(ymd))));
    return errc::ok;
}

inline errc deserialize_binary_value_datetime(
    deserialization_context& ctx,
    value& output
) noexcept
{
    using namespace binc;

    // Deserialize length
    std::uint8_t length;
    auto err = deserialize(ctx, length);
    if (err != errc::ok)
        return err;

    // Deserialize date. If the DATETIME does not contain these values,
    // they are supposed to be zero (invalid date)
    year_month_day ymd {};
    if (length >= datetime_d_sz)
    {
        err = deserialize_binary_ymd(ctx, ymd);
        if (err != errc::ok)
            return err;
    }

    // If the DATETIME contains no value for these fields, they are zero
    std::uint8_t hours = 0;
    std::uint8_t minutes = 0;
    std::uint8_t seconds = 0;
    std::uint32_t micros = 0;

    // Hours, minutes, seconds
    if (length >= datetime_dhms_sz)
    {
        err = deserialize(ctx, hours, minutes, seconds);
        if (err != errc::ok)
            return err;
    }

    // Microseconds
    if (length >= datetime_dhmsu_sz)
    {
        err = deserialize(ctx, micros);
        if (err != errc::ok)
            return err;
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

    // Check for invalid dates, represented in C++ as NULL.
    // Note: we do the check here to ensure we consume all the bytes
    // associated to this datetime
    if (!is_valid(ymd))
    {
        output = value(nullptr);
        return errc::ok;
    }

    // Compose the final datetime. Doing time of day and date separately to avoid overflow
    date d (days(ymd_to_days(ymd)));
    auto time_of_day =
        std::chrono::hours(hours) +
        std::chrono::minutes(minutes) +
        std::chrono::seconds(seconds) +
        std::chrono::microseconds(micros);
    output = value(d + time_of_day);
    return errc::ok;
}

inline errc deserialize_binary_value_time(
    deserialization_context& ctx,
    value& output
) noexcept
{
    using namespace binc;

    // Deserialize length
    std::uint8_t length;
    auto err = deserialize(ctx, length);
    if (err != errc::ok)
        return err;

    // If the TIME contains no value for these fields, they are zero
    std::uint8_t is_negative = 0;
    std::uint32_t num_days = 0;
    std::uint8_t hours = 0;
    std::uint8_t minutes = 0;
    std::uint8_t seconds = 0;
    std::uint32_t microseconds = 0;

    // Sign, days, hours, minutes, seconds
    if (length >= time_dhms_sz)
    {
        err = deserialize(
            ctx,
            is_negative,
            num_days,
            hours,
            minutes,
            seconds
        );
        if (err != errc::ok)
            return err;
    }

    // Microseconds
    if (length >= time_dhmsu_sz)
    {
        err = deserialize(ctx, microseconds);
        if (err != errc::ok)
            return err;
    }

    // Range check
    if (num_days > time_max_days ||
        hours > max_hour ||
        minutes > max_min ||
        seconds > max_sec ||
        microseconds > max_micro)
    {
        return errc::protocol_value_error;
    }

    // Compose the final time
    output = value(time((is_negative ? -1 : 1) * (
         days(num_days) +
         std::chrono::hours(hours) +
         std::chrono::minutes(minutes) +
         std::chrono::seconds(seconds) +
         std::chrono::microseconds(microseconds)
    )));
    return errc::ok;
}


} // detail
} // mysql
} // boost

inline boost::mysql::errc boost::mysql::detail::deserialize_binary_value(
    deserialization_context& ctx,
    const field_metadata& meta,
    value& output
)
{
    switch (meta.protocol_type())
    {
    case protocol_field_type::tiny:
        return deserialize_binary_value_int<std::uint8_t, std::int8_t>(meta, ctx, output);
    case protocol_field_type::short_:
    case protocol_field_type::year:
        return deserialize_binary_value_int<std::uint16_t, std::int16_t>(meta, ctx, output);
    case protocol_field_type::int24:
    case protocol_field_type::long_:
        return deserialize_binary_value_int<std::uint32_t, std::int32_t>(meta, ctx, output);
    case protocol_field_type::longlong:
        return deserialize_binary_value_int<std::uint64_t, std::int64_t>(meta, ctx, output);
    case protocol_field_type::bit:
        return deserialize_binary_value_bit(ctx, output);
    case protocol_field_type::float_:
        return deserialize_binary_value_float<float>(ctx, output);
    case protocol_field_type::double_:
        return deserialize_binary_value_float<double>(ctx, output);
    case protocol_field_type::timestamp:
    case protocol_field_type::datetime:
        return deserialize_binary_value_datetime(ctx, output);
    case protocol_field_type::date:
        return deserialize_binary_value_date(ctx, output);
    case protocol_field_type::time:
        return deserialize_binary_value_time(ctx, output);
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
        return deserialize_binary_value_string(ctx, output);
    }
}

inline boost::mysql::error_code boost::mysql::detail::deserialize_binary_row(
    deserialization_context& ctx,
    const std::vector<field_metadata>& meta,
    std::vector<value>& output
)
{
    // Skip packet header (it is not part of the message in the binary
    // protocol but it is in the text protocol, so we include it for homogeneity)
    // The caller will have checked we have this byte already for us
    assert(ctx.enough_size(1));
    ctx.advance(1);

    // Number of fields
    auto num_fields = meta.size();
    output.resize(num_fields);

    // Null bitmap
    null_bitmap_traits null_bitmap (binary_row_null_bitmap_offset, num_fields);
    const std::uint8_t* null_bitmap_begin = ctx.first();
    if (!ctx.enough_size(null_bitmap.byte_count()))
        return make_error_code(errc::incomplete_message);
    ctx.advance(null_bitmap.byte_count());

    // Actual values
    for (std::vector<value>::size_type i = 0; i < output.size(); ++i)
    {
        if (null_bitmap.is_null(null_bitmap_begin, i))
        {
            output[i] = value(nullptr);
        }
        else
        {
            auto err = deserialize_binary_value(ctx, meta[i], output[i]);
            if (err != errc::ok)
                return make_error_code(err);
        }
    }

    // Check for remaining bytes
    if (!ctx.empty())
        return make_error_code(errc::extra_bytes);

    return error_code();
}


#endif /* INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_BINARY_DESERIALIZATION_IPP_ */
