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
#include "boost/mysql/detail/protocol/constants.hpp"
#include "boost/mysql/detail/auxiliar/tmp.hpp"

namespace boost {
namespace mysql {
namespace detail {

// ints and strings
template <typename TargetType, typename DeserializableType=TargetType>
errc deserialize_binary_value_value_holder(
    deserialization_context& ctx,
    value& output
) noexcept
{
    DeserializableType deser;
    auto err = deserialize(ctx, deser);
    if (err != errc::ok)
        return err;
    output = value(static_cast<TargetType>(deser.value));
    return errc::ok;
}

template <
    typename DeserializableTypeUnsigned,
    typename DeserializableTypeSigned
>
errc deserialize_binary_value_int(
    const field_metadata& meta,
    deserialization_context& ctx,
    value& output
) noexcept
{
    return meta.is_unsigned() ?
        deserialize_binary_value_value_holder<
            std::uint64_t, DeserializableTypeUnsigned>(ctx, output) :
        deserialize_binary_value_value_holder<
            std::int64_t, DeserializableTypeSigned>(ctx, output);
}

// Floats
template <typename T>
errc deserialize_binary_value_float(
    deserialization_context& ctx,
    value& output
) noexcept
{
    // Size check
    if (!ctx.enough_size(sizeof(T)))
        return errc::incomplete_message;

    // Endianness conversion. Boost.Endian support for floats start at 1.71
    T v;
#if BOOST_ENDIAN_BIG_BYTE
    char buf [sizeof(T)];
    std::memcpy(buf, ctx.first(), sizeof(T));
    std::reverse(buf, buf + sizeof(T));
    std::memcpy(&v, buf, sizeof(T));
#else
    std::memcpy(&v, ctx.first(), sizeof(T));
#endif

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
    ::date::year_month_day& output
)
{
    int2 year;
    int1 month;
    int1 day;

    // Deserialize
    auto err = deserialize(ctx, year, month, day);
    if (err != errc::ok)
        return err;

    // Range check
    if (year.value > max_year ||
        month.value > max_month ||
        day.value > max_day)
    {
        return errc::protocol_value_error;
    }

    output = ::date::year_month_day (
        ::date::year(year.value),
        ::date::month(month.value),
        ::date::day(day.value)
    );

    return errc::ok;
}

inline errc deserialize_binary_value_date(
    deserialization_context& ctx,
    value& output
) noexcept
{
    // Deserialize length
    int1 length;
    auto err = deserialize(ctx, length);
    if (err != errc::ok)
        return err;

    // Check for zero dates, represented in C++ as a NULL
    if (length.value < binc::date_sz)
    {
        output = value(nullptr);
        return errc::ok;
    }

    // Deserialize rest of fields
    ::date::year_month_day ymd;
    err = deserialize_binary_ymd(ctx, ymd);
    if (err != errc::ok)
        return err;

    // Check for invalid dates, represented as NULL in C++
    if (!ymd.ok())
    {
        output = value(nullptr);
        return errc::ok;
    }

    // Range check
    date d (ymd);
    if (is_out_of_range(d))
        return errc::protocol_value_error;

    // Convert to sys_days (date)
    output = value(static_cast<date>(ymd));
    return errc::ok;
}

inline errc deserialize_binary_value_datetime(
    deserialization_context& ctx,
    value& output
) noexcept
{
    using namespace binc;

    // Deserialize length
    int1 length;
    auto err = deserialize(ctx, length);
    if (err != errc::ok)
        return err;

    // Deserialize date. If the DATETIME does not contain these values,
    // they are supposed to be zero (invalid date)
    ::date::year_month_day ymd (::date::year(0), ::date::month(0), ::date::day(0));
    if (length.value >= datetime_d_sz)
    {
        err = deserialize_binary_ymd(ctx, ymd);
        if (err != errc::ok)
            return err;
    }

    // If the DATETIME contains no value for these fields, they are zero
    int1 hours (0);
    int1 minutes (0);
    int1 seconds (0);
    int4 micros (0);

    // Hours, minutes, seconds
    if (length.value >= datetime_dhms_sz)
    {
        err = deserialize(ctx, hours, minutes, seconds);
        if (err != errc::ok)
            return err;
    }

    // Microseconds
    if (length.value >= datetime_dhmsu_sz)
    {
        err = deserialize(ctx, micros);
        if (err != errc::ok)
            return err;
    }

    // Validity check. We make this check before
    // the invalid date check to make invalid dates with incorrect
    // hours/mins/secs/micros fail
    if (hours.value > max_hour ||
        minutes.value > max_min ||
        seconds.value > max_sec ||
        micros.value > max_micro)
    {
        return errc::protocol_value_error;
    }

    // Check for invalid dates, represented in C++ as NULL.
    // Note: we do the check here to ensure we consume all the bytes
    // associated to this datetime
    if (!ymd.ok())
    {
        output = value(nullptr);
        return errc::ok;
    }

    // Range check
    date d (ymd);
    if (is_out_of_range(d))
    {
        return errc::protocol_value_error;
    }

    // Compose the final datetime. Doing time of day and date separately to avoid overflow
    auto time_of_day =
        std::chrono::hours(hours.value) +
        std::chrono::minutes(minutes.value) +
        std::chrono::seconds(seconds.value) +
        std::chrono::microseconds(micros.value);
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
    int1 length;
    auto err = deserialize(ctx, length);
    if (err != errc::ok)
        return err;

    // If the TIME contains no value for these fields, they are zero
    int1 is_negative (0);
    int4 days (0);
    int1 hours (0);
    int1 minutes(0);
    int1 seconds(0);
    int4 microseconds(0);

    // Sign, days, hours, minutes, seconds
    if (length.value >= time_dhms_sz)
    {
        err = deserialize(
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

    // Microseconds
    if (length.value >= time_dhmsu_sz)
    {
        err = deserialize(ctx, microseconds);
        if (err != errc::ok)
            return err;
    }

    // Range check
    if (days.value > time_max_days ||
        hours.value > max_hour ||
        minutes.value > max_min ||
        seconds.value > max_sec ||
        microseconds.value > max_micro)
    {
        return errc::protocol_value_error;
    }

    // Compose the final time
    output = value(time((is_negative.value ? -1 : 1) * (
         ::date::days(days.value) +
         std::chrono::hours(hours.value) +
         std::chrono::minutes(minutes.value) +
         std::chrono::seconds(seconds.value) +
         std::chrono::microseconds(microseconds.value)
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
        return deserialize_binary_value_int<int1, int1_signed>(meta, ctx, output);
    case protocol_field_type::short_:
    case protocol_field_type::year:
        return deserialize_binary_value_int<int2, int2_signed>(meta, ctx, output);
    case protocol_field_type::int24:
    case protocol_field_type::long_:
        return deserialize_binary_value_int<int4, int4_signed>(meta, ctx, output);
    case protocol_field_type::longlong:
        return deserialize_binary_value_int<int8, int8_signed>(meta, ctx, output);
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
    case protocol_field_type::bit:
    case protocol_field_type::newdecimal:
    case protocol_field_type::geometry:
    default:
        return deserialize_binary_value_value_holder<std::string_view, string_lenenc>(ctx, output);
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
