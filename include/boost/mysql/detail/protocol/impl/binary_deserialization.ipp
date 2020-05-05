//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_BINARY_DESERIALIZATION_IPP
#define BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_BINARY_DESERIALIZATION_IPP

#include <variant>
#include "boost/mysql/detail/protocol/serialization.hpp"
#include "boost/mysql/detail/protocol/null_bitmap_traits.hpp"
#include "boost/mysql/detail/auxiliar/tmp.hpp"

namespace boost {
namespace mysql {
namespace detail {

// ints and strings
template <typename TargetType, typename DeserializableType=TargetType>
errc deserialize_binary_value_to_variant_value_holder(
    deserialization_context& ctx,
    value& output
) noexcept
{
    DeserializableType deser;
    auto err = deserialize(deser, ctx);
    if (err != errc::ok)
        return err;
    output = TargetType(deser.value);
    return errc::ok;
}

template <
    typename TargetTypeUnsigned,
    typename TargetTypeSigned,
    typename DeserializableTypeUnsigned,
    typename DeserializableTypeSigned
>
errc deserialize_binary_value_to_variant_int(
    const field_metadata& meta,
    deserialization_context& ctx,
    value& output
) noexcept
{
    return meta.is_unsigned() ?
        deserialize_binary_value_to_variant_value_holder<
            TargetTypeUnsigned, DeserializableTypeUnsigned>(ctx, output) :
        deserialize_binary_value_to_variant_value_holder<
            TargetTypeSigned, DeserializableTypeSigned>(ctx, output);
}

// Floats
template <typename T>
errc deserialize_binary_value_to_variant_float(
    deserialization_context& ctx,
    value& output
) noexcept
{
    // Size check
    if (!ctx.enough_size(sizeof(T)))
        return errc::incomplete_message;

    T v;

    // Endianness conversion. Boost.Endian support for floats start at 1.71
#if BOOST_ENDIAN_BIG_BYTE
    char buf [sizeof(T)];
    std::memcpy(buf, ctx.first(), sizeof(T));
    std::reverse(buf, buf + sizeof(T));
    std::memcpy(&v, buf, sizeof(T));
#else
    std::memcpy(&v, ctx.first(), sizeof(T));
#endif
    ctx.advance(sizeof(T));
    if (std::isnan(v) || std::isinf(v))
    {
        //output = nullptr;
        return errc::protocol_value_error;
    }
    else
    {
        output = v;
    }
    return errc::ok;
}

// Time types
inline errc deserialize_binary_value_to_variant_date(
    deserialization_context& ctx,
    value& output
) noexcept
{
    int1 length;
    int2 year;
    int1 month;
    int1 day;

    // Deserialize length
    auto err = deserialize(length, ctx);
    if (err != errc::ok)
        return err;

    // A zero date. This is represented in C++ as a NULL
    if (length.value < 4)
    {
        output = nullptr;
        return errc::ok;
    }

    // Deserialize rest of fields
    err = deserialize_fields(ctx, year, month, day);
    if (err != errc::ok)
        return err;

    ::date::year_month_day ymd (::date::year(year.value), ::date::month(month.value), ::date::day(day.value));
    if (!ymd.ok()) // invalid date. We represent this as a NULL
    {
        output = nullptr;
        return errc::ok;
    }

    date d (ymd);
    if (d < min_date || d > max_date)
        return errc::protocol_value_error;

    output = d;
    return errc::ok;
}

inline errc deserialize_binary_value_to_variant_datetime(
    deserialization_context& ctx,
    value& output
) noexcept
{
    int1 length;
    int2 year;
    int1 month;
    int1 day;
    int1 hours;
    int1 minutes;
    int1 seconds;
    int4 micros;

    // Deserialize length
    auto err = deserialize(length, ctx);
    if (err != errc::ok)
        return err;

    // A zero datetime, represented as NULL in C++
    if (length.value < 4)
    {
        output = nullptr;
        return errc::ok;
    }

    // Date part
    err = deserialize_fields(ctx, year, month, day);
    if (err != errc::ok)
        return err;
    ::date::year_month_day ymd (::date::year(year.value), ::date::month(month.value), ::date::day(day.value));
    if (!ymd.ok())
    {
        output = nullptr;
        return errc::ok;
    }

    // Rest of fields
    if (length.value >= 7)
    {
        err = deserialize_fields(ctx, hours, minutes, seconds);
        if (err != errc::ok)
            return err;
    }
    if (length.value >= 11)
    {
        err = deserialize(micros, ctx);
        if (err != errc::ok)
            return err;
    }

    // TODO: range check

    // Compose the final datetime. Doing time of day and date separately to avoid overflow
    auto time_of_day_part = std::chrono::hours(hours.value) + std::chrono::minutes(minutes.value) +
        std::chrono::seconds(seconds.value) + std::chrono::microseconds(micros.value);
    output = date(ymd) + time_of_day_part;
    return errc::ok;
}

inline errc deserialize_binary_value_to_variant_time(
    deserialization_context& ctx,
    value& output
) noexcept
{
    // Length
    int1 length;
    auto err = deserialize(length, ctx);
    if (err != errc::ok)
        return err;

    int1 is_negative (0);
    int4 days (0);
    int1 hours (0);
    int1 minutes(0);
    int1 seconds(0);
    int4 microseconds(0);

    if (length.value >= 8)
    {
        err = deserialize_fields(
            ctx,
            is_negative,
            days,
            hours,
            minutes,
            seconds
        );
        if (err != errc::ok)
            return err;
    }
    if (length.value >= 12)
    {
        err = deserialize(microseconds, ctx);
        if (err != errc::ok)
            return err;
    }

    output = (is_negative.value ? -1 : 1) * (
         ::date::days(days.value) +
         std::chrono::hours(hours.value) +
         std::chrono::minutes(minutes.value) +
         std::chrono::seconds(seconds.value) +
         std::chrono::microseconds(microseconds.value)
    );
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
        return deserialize_binary_value_to_variant_int<
                std::uint32_t, std::int32_t, int1, int1_signed>(meta, ctx, output);
    case protocol_field_type::short_:
    case protocol_field_type::year:
        return deserialize_binary_value_to_variant_int<
                std::uint32_t, std::int32_t, int2, int2_signed>(meta, ctx, output);
    case protocol_field_type::int24:
    case protocol_field_type::long_:
        return deserialize_binary_value_to_variant_int<
                std::uint32_t, std::int32_t, int4, int4_signed>(meta, ctx, output);
    case protocol_field_type::longlong:
        return deserialize_binary_value_to_variant_int<
                std::uint64_t, std::int64_t, int8, int8_signed>(meta, ctx, output);
    case protocol_field_type::float_:
        return deserialize_binary_value_to_variant_float<float>(ctx, output);
    case protocol_field_type::double_:
        return deserialize_binary_value_to_variant_float<double>(ctx, output);
    case protocol_field_type::timestamp:
    case protocol_field_type::datetime:
        return deserialize_binary_value_to_variant_datetime(ctx, output);
    case protocol_field_type::date:
        return deserialize_binary_value_to_variant_date(ctx, output);
    case protocol_field_type::time:
        return deserialize_binary_value_to_variant_time(ctx, output);
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
        return deserialize_binary_value_to_variant_value_holder<std::string_view, string_lenenc>(ctx, output);
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
            output[i] = nullptr;
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
