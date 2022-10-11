//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_DESERIALIZE_BINARY_FIELD_IPP
#define BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_DESERIALIZE_BINARY_FIELD_IPP

#pragma once

#include <boost/mysql/detail/auxiliar/string_view_offset.hpp>
#include <boost/mysql/detail/protocol/bit_deserialization.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>
#include <boost/mysql/detail/protocol/date.hpp>
#include <boost/mysql/detail/protocol/deserialize_binary_field.hpp>
#include <boost/mysql/detail/protocol/serialization.hpp>

#include <cstddef>

namespace boost {
namespace mysql {
namespace detail {

// strings
inline errc deserialize_binary_field_string(
    deserialization_context& ctx,
    field_view& output,
    const std::uint8_t* buffer_first
) noexcept
{
    string_lenenc deser;
    auto err = deserialize(ctx, deser);
    if (err != errc::ok)
        return err;
    output = field_view(detail::string_view_offset::from_sv(deser.value, buffer_first));
    return errc::ok;
}

// ints
template <class TargetType, class DeserializableType>
errc deserialize_binary_field_int_impl(deserialization_context& ctx, field_view& output) noexcept
{
    DeserializableType deser;
    auto err = deserialize(ctx, deser);
    if (err != errc::ok)
        return err;
    output = field_view(static_cast<TargetType>(deser));
    return errc::ok;
}

template <class DeserializableTypeUnsigned, class DeserializableTypeSigned>
errc deserialize_binary_field_int(
    const metadata& meta,
    deserialization_context& ctx,
    field_view& output
) noexcept
{
    return meta.is_unsigned()
               ? deserialize_binary_field_int_impl<std::uint64_t, DeserializableTypeUnsigned>(
                     ctx,
                     output
                 )
               : deserialize_binary_field_int_impl<std::int64_t, DeserializableTypeSigned>(
                     ctx,
                     output
                 );
}

// Bits. These come as a binary value between 1 and 8 bytes,
// packed in a string
inline errc deserialize_binary_field_bit(deserialization_context& ctx, field_view& output) noexcept
{
    string_lenenc buffer;
    auto err = deserialize(ctx, buffer);
    if (err != errc::ok)
        return err;
    return deserialize_bit(buffer.value, output);
}

// Floats
template <class T>
errc deserialize_binary_field_float(deserialization_context& ctx, field_view& output) noexcept
{
    // Size check
    if (!ctx.enough_size(sizeof(T)))
        return errc::incomplete_message;

    // Endianness conversion. Boost.Endian support for floats start at 1.71
    T v = boost::endian::endian_load<T, sizeof(T), boost::endian::order::little>(ctx.first());

    // Nans and infs not allowed in SQL
    if (std::isnan(v) || std::isinf(v))
        return errc::protocol_value_error;

    // Done
    ctx.advance(sizeof(T));
    output = field_view(v);
    return errc::ok;
}

// Time types
inline errc deserialize_binary_ymd(deserialization_context& ctx, year_month_day& output)
{
    std::uint16_t year;
    std::uint8_t month;
    std::uint8_t day;

    // Deserialize
    auto err = deserialize(ctx, year, month, day);
    if (err != errc::ok)
        return err;

    // Range check
    if (year > max_year || month > max_month || day > max_day)
    {
        return errc::protocol_value_error;
    }

    output = year_month_day{year, month, day};

    return errc::ok;
}

inline errc deserialize_binary_field_date(deserialization_context& ctx, field_view& output) noexcept
{
    // Deserialize length
    std::uint8_t length;
    auto err = deserialize(ctx, length);
    if (err != errc::ok)
        return err;

    // Check for zero dates, represented in C++ as a NULL
    if (length < binc::date_sz)
    {
        output = field_view(nullptr);
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
        output = field_view(nullptr);
        return errc::ok;
    }

    // Convert to value
    output = field_view(date(days(ymd_to_days(ymd))));
    return errc::ok;
}

inline errc
deserialize_binary_field_datetime(deserialization_context& ctx, field_view& output) noexcept
{
    using namespace binc;

    // Deserialize length
    std::uint8_t length;
    auto err = deserialize(ctx, length);
    if (err != errc::ok)
        return err;

    // Deserialize date. If the DATETIME does not contain these values,
    // they are supposed to be zero (invalid date)
    year_month_day ymd{};
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
    if (hours > max_hour || minutes > max_min || seconds > max_sec || micros > max_micro)
    {
        return errc::protocol_value_error;
    }

    // Check for invalid dates, represented in C++ as NULL.
    // Note: we do the check here to ensure we consume all the bytes
    // associated to this datetime
    if (!is_valid(ymd))
    {
        output = field_view(nullptr);
        return errc::ok;
    }

    // Compose the final datetime. Doing time of day and date separately to avoid overflow
    date d(days(ymd_to_days(ymd)));
    auto time_of_day = std::chrono::hours(hours) + std::chrono::minutes(minutes) +
                       std::chrono::seconds(seconds) + std::chrono::microseconds(micros);
    output = field_view(d + time_of_day);
    return errc::ok;
}

inline errc deserialize_binary_field_time(deserialization_context& ctx, field_view& output) noexcept
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
        err = deserialize(ctx, is_negative, num_days, hours, minutes, seconds);
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
    if (num_days > time_max_days || hours > max_hour || minutes > max_min || seconds > max_sec ||
        microseconds > max_micro)
    {
        return errc::protocol_value_error;
    }

    // Compose the final time
    output = field_view(time(
        (is_negative ? -1 : 1) *
        (days(num_days) + std::chrono::hours(hours) + std::chrono::minutes(minutes) +
         std::chrono::seconds(seconds) + std::chrono::microseconds(microseconds))
    ));
    return errc::ok;
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

inline boost::mysql::errc boost::mysql::detail::deserialize_binary_field(
    deserialization_context& ctx,
    const metadata& meta,
    const std::uint8_t* buffer_first,
    field_view& output
)
{
    switch (meta.protocol_type())
    {
    case protocol_field_type::tiny:
        return deserialize_binary_field_int<std::uint8_t, std::int8_t>(meta, ctx, output);
    case protocol_field_type::short_:
    case protocol_field_type::year:
        return deserialize_binary_field_int<std::uint16_t, std::int16_t>(meta, ctx, output);
    case protocol_field_type::int24:
    case protocol_field_type::long_:
        return deserialize_binary_field_int<std::uint32_t, std::int32_t>(meta, ctx, output);
    case protocol_field_type::longlong:
        return deserialize_binary_field_int<std::uint64_t, std::int64_t>(meta, ctx, output);
    case protocol_field_type::bit: return deserialize_binary_field_bit(ctx, output);
    case protocol_field_type::float_: return deserialize_binary_field_float<float>(ctx, output);
    case protocol_field_type::double_: return deserialize_binary_field_float<double>(ctx, output);
    case protocol_field_type::timestamp:
    case protocol_field_type::datetime: return deserialize_binary_field_datetime(ctx, output);
    case protocol_field_type::date: return deserialize_binary_field_date(ctx, output);
    case protocol_field_type::time: return deserialize_binary_field_time(ctx, output);
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
    default: return deserialize_binary_field_string(ctx, output, buffer_first);
    }
}

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_BINARY_DESERIALIZATION_IPP_ */
