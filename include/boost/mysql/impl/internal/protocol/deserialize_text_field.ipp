//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_DESERIALIZE_TEXT_FIELD_IPP
#define BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_DESERIALIZE_TEXT_FIELD_IPP

#pragma once

#include <boost/mysql/blob_view.hpp>
#include <boost/mysql/datetime.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/metadata.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/datetime.hpp>

#include <boost/mysql/impl/internal/protocol/bit_deserialization.hpp>
#include <boost/mysql/impl/internal/protocol/constants.hpp>
#include <boost/mysql/impl/internal/protocol/deserialize_text_field.hpp>
#include <boost/mysql/impl/internal/protocol/serialization.hpp>

#include <boost/assert.hpp>

#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <system_error>

namespace boost {
namespace mysql {
namespace detail {

// Constants
BOOST_MYSQL_STATIC_IF_COMPILED constexpr unsigned max_decimals = 6u;

namespace textc {
BOOST_MYSQL_STATIC_IF_COMPILED constexpr std::size_t year_sz = 4;
BOOST_MYSQL_STATIC_IF_COMPILED constexpr std::size_t month_sz = 2;
BOOST_MYSQL_STATIC_IF_COMPILED constexpr std::size_t day_sz = 2;
BOOST_MYSQL_STATIC_IF_COMPILED constexpr std::size_t hours_min_sz = 2;  // in TIME, it may be longer
BOOST_MYSQL_STATIC_IF_COMPILED constexpr std::size_t mins_sz = 2;
BOOST_MYSQL_STATIC_IF_COMPILED constexpr std::size_t secs_sz = 2;

BOOST_MYSQL_STATIC_IF_COMPILED constexpr std::size_t date_sz = year_sz + month_sz + day_sz + 2;  // delimiters
BOOST_MYSQL_STATIC_IF_COMPILED constexpr std::size_t time_min_sz = hours_min_sz + mins_sz + secs_sz +
                                                                   2;  // delimiters
BOOST_MYSQL_STATIC_IF_COMPILED constexpr std::size_t time_max_sz = time_min_sz + max_decimals +
                                                                   3;  // sign, period, hour extra character
BOOST_MYSQL_STATIC_IF_COMPILED constexpr std::size_t datetime_min_sz = date_sz + time_min_sz +
                                                                       1;  // delimiter
BOOST_MYSQL_STATIC_IF_COMPILED constexpr std::size_t datetime_max_sz = datetime_min_sz + max_decimals +
                                                                       1;  // period

BOOST_MYSQL_STATIC_IF_COMPILED constexpr unsigned time_max_hour = 838;
}  // namespace textc

// Integers
template <class T>
BOOST_MYSQL_STATIC_OR_INLINE deserialize_errc
deserialize_text_value_int_impl(string_view from, field_view& to) noexcept
{
    // Iterators
    const char* begin = from.data();
    const char* end = begin + from.size();

    // Convert
    T v;
    std::from_chars_result res = std::from_chars(from.data(), from.data() + from.size(), v);

    // Check
    if (res.ec != std::errc() || res.ptr != end)
        return deserialize_errc::protocol_value_error;

    // Done
    to = field_view(v);
    return deserialize_errc::ok;
}

BOOST_MYSQL_STATIC_OR_INLINE deserialize_errc
deserialize_text_value_int(string_view from, field_view& to, const metadata& meta) noexcept
{
    return meta.is_unsigned() ? deserialize_text_value_int_impl<std::uint64_t>(from, to)
                              : deserialize_text_value_int_impl<std::int64_t>(from, to);
}

// Floating points
template <class T>
BOOST_MYSQL_STATIC_OR_INLINE deserialize_errc
deserialize_text_value_float(string_view from, field_view& to) noexcept
{
    // Iterators
    const char* begin = from.data();
    const char* end = begin + from.size();

    // Convert
    T val;
    std::from_chars_result res = std::from_chars(begin, end, val);

    // Check. SQL std forbids nan and inf
    if (res.ec != std::errc() || res.ptr != end || std::isnan(val) || std::isinf(val))
        return deserialize_errc::protocol_value_error;

    // Done
    to = field_view(val);
    return deserialize_errc::ok;
}

// Strings
BOOST_MYSQL_STATIC_OR_INLINE deserialize_errc
deserialize_text_value_string(string_view from, field_view& to) noexcept
{
    to = field_view(from);
    return deserialize_errc::ok;
}

BOOST_MYSQL_STATIC_OR_INLINE deserialize_errc
deserialize_text_value_blob(string_view from, field_view& to) noexcept
{
    to = field_view(to_span(from));
    return deserialize_errc::ok;
}

// Date/time types
BOOST_MYSQL_STATIC_OR_INLINE unsigned sanitize_decimals(unsigned decimals) noexcept
{
    return (std::min)(decimals, max_decimals);
}

BOOST_MYSQL_STATIC_OR_INLINE deserialize_errc deserialize_text_ymd(string_view from, date& to)
{
    using namespace textc;

    // Size check
    if (from.size() != date_sz)
        return deserialize_errc::protocol_value_error;

    // Iterators
    const char* it = from.data();
    const char* end = it + from.size();

    // Year
    std::uint16_t year = 0;
    auto res = std::from_chars(it, end, year);
    if (res.ec != std::errc() || res.ptr != it + year_sz)
        return deserialize_errc::protocol_value_error;
    it += year_sz;

    // Separator
    BOOST_ASSERT(it != end);
    if (*it++ != '-')
        return deserialize_errc::protocol_value_error;

    // Month
    std::uint8_t month = 0;
    res = std::from_chars(it, end, month);
    if (res.ec != std::errc() || res.ptr != it + month_sz)
        return deserialize_errc::protocol_value_error;
    it += month_sz;

    // Separator
    BOOST_ASSERT(it != end);
    if (*it++ != '-')
        return deserialize_errc::protocol_value_error;

    // Day
    std::uint8_t day = 0;
    res = std::from_chars(it, end, day);
    if (res.ec != std::errc() || res.ptr != it + day_sz)
        return deserialize_errc::protocol_value_error;
    BOOST_ASSERT(it + day_sz == end);

    // Range check for individual components. MySQL doesn't allow invidiual components
    // to be out of range, although they may be zero or representing an invalid date
    if (year > max_year || month > max_month || day > max_day)
        return deserialize_errc::protocol_value_error;

    to = date(year, month, day);
    return deserialize_errc::ok;
}

BOOST_MYSQL_STATIC_OR_INLINE deserialize_errc
deserialize_text_value_date(string_view from, field_view& to) noexcept
{
    date d;
    auto err = deserialize_text_ymd(from, d);
    if (err != deserialize_errc::ok)
        return err;
    to = field_view(d);
    return deserialize_errc::ok;
}

BOOST_MYSQL_STATIC_OR_INLINE deserialize_errc
deserialize_text_value_datetime(string_view from, field_view& to, const metadata& meta) noexcept
{
    using namespace textc;

    // Sanitize decimals
    unsigned decimals = sanitize_decimals(meta.decimals());

    // Length check
    std::size_t expected_size = datetime_min_sz + (decimals ? decimals + 1 : 0);
    if (from.size() != expected_size)
        return deserialize_errc::protocol_value_error;

    // Deserialize date part
    date d;
    auto err = deserialize_text_ymd(from.substr(0, date_sz), d);
    if (err != deserialize_errc::ok)
        return err;

    // Iterators
    const char* it = from.data() + date_sz;
    const char* end = from.data() + from.size();

    // Separator
    BOOST_ASSERT(it != end);
    if (*it++ != ' ')
        return deserialize_errc::protocol_value_error;

    // Hour
    std::uint8_t hour = 0;
    auto res = std::from_chars(it, end, hour);
    if (res.ec != std::errc() || res.ptr != it + 2)
        return deserialize_errc::protocol_value_error;
    it += 2;

    // Separator
    BOOST_ASSERT(it != end);
    if (*it++ != ':')
        return deserialize_errc::protocol_value_error;

    // Minute
    std::uint8_t minute = 0;
    res = std::from_chars(it, end, minute);
    if (res.ec != std::errc() || res.ptr != it + 2)
        return deserialize_errc::protocol_value_error;
    it += 2;

    // Separator
    BOOST_ASSERT(it != end);
    if (*it++ != ':')
        return deserialize_errc::protocol_value_error;

    // Second
    std::uint8_t second = 0;
    res = std::from_chars(it, end, second);
    if (res.ec != std::errc() || res.ptr != it + 2)
        return deserialize_errc::protocol_value_error;
    it += 2;

    std::uint32_t microsecond = 0;

    if (decimals)
    {
        // Microsecond separator
        BOOST_ASSERT(it != end);
        if (*it++ != '.')
            return deserialize_errc::protocol_value_error;

        // Microseconds
        auto micros_num_chars = end - it;
        if (micros_num_chars != decimals)
            return deserialize_errc::protocol_value_error;
        char micros_buff[6] = {'0', '0', '0', '0', '0', '0'};
        std::memcpy(micros_buff, it, micros_num_chars);
        res = std::from_chars(micros_buff, micros_buff + 6, microsecond);
        if (res.ec != std::errc() || res.ptr != micros_buff + 6)
            return deserialize_errc::protocol_value_error;
    }

    // Validity check. Although MySQL allows invalid and zero datetimes, it doesn't allow
    // individual components to be out of range.
    if (hour > max_hour || minute > max_min || second > max_sec || microsecond > max_micro)
    {
        return deserialize_errc::protocol_value_error;
    }

    to = field_view(datetime(d.year(), d.month(), d.day(), hour, minute, second, microsecond));
    return deserialize_errc::ok;
}

BOOST_MYSQL_STATIC_OR_INLINE deserialize_errc
deserialize_text_value_time(string_view from, field_view& to, const metadata& meta) noexcept
{
    using namespace textc;

    // Sanitize decimals
    unsigned decimals = sanitize_decimals(meta.decimals());

    // Iterators
    const char* it = from.data();
    const char* end = it + from.size();

    // Sign
    if (it == end)
        return deserialize_errc::protocol_value_error;
    bool is_negative = *it == '-';
    if (is_negative)
        ++it;

    // Hours
    std::uint16_t hours = 0;
    auto res = std::from_chars(it, end, hours);
    if (res.ec != std::errc())
        return deserialize_errc::protocol_value_error;
    auto hour_num_chars = res.ptr - it;
    if (hour_num_chars != 2 && hour_num_chars != 3)  // may take between 2 and 3 chars
        return deserialize_errc::protocol_value_error;
    it = res.ptr;

    // Separator
    if (it == end || *it++ != ':')
        return deserialize_errc::protocol_value_error;

    // Minute
    std::uint8_t minute = 0;
    res = std::from_chars(it, end, minute);
    if (res.ec != std::errc() || res.ptr != it + 2)
        return deserialize_errc::protocol_value_error;
    it += 2;

    // Separator
    if (it == end || *it++ != ':')
        return deserialize_errc::protocol_value_error;

    // Second
    std::uint8_t second = 0;
    res = std::from_chars(it, end, second);
    if (res.ec != std::errc() || res.ptr != it + 2)
        return deserialize_errc::protocol_value_error;
    it += 2;

    // Microsecond
    std::uint32_t microsecond = 0;

    if (decimals)
    {
        // Separator
        if (it == end || *it++ != '.')
            return deserialize_errc::protocol_value_error;

        // Microseconds
        auto micros_num_chars = end - it;
        if (micros_num_chars != decimals)
            return deserialize_errc::protocol_value_error;
        char micros_buff[6] = {'0', '0', '0', '0', '0', '0'};
        std::memcpy(micros_buff, it, micros_num_chars);
        it += micros_num_chars;
        res = std::from_chars(micros_buff, micros_buff + 6, microsecond);
        if (res.ec != std::errc() || res.ptr != micros_buff + 6)
            return deserialize_errc::protocol_value_error;
    }

    // Size check
    if (it != end)
        return deserialize_errc::protocol_value_error;

    // Range check
    if (hours > time_max_hour || minute > max_min || second > max_sec || microsecond > max_micro)
        return deserialize_errc::protocol_value_error;

    // Sum it
    auto t = std::chrono::hours(hours) + std::chrono::minutes(minute) + std::chrono::seconds(second) +
             std::chrono::microseconds(microsecond);
    if (is_negative)
    {
        t = -t;
    }

    // Done
    to = field_view(t);
    return deserialize_errc::ok;
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

boost::mysql::detail::deserialize_errc boost::mysql::detail::deserialize_text_field(
    string_view from,
    const metadata& meta,
    field_view& output
)
{
    switch (meta.type())
    {
    case column_type::tinyint:
    case column_type::smallint:
    case column_type::mediumint:
    case column_type::int_:
    case column_type::bigint:
    case column_type::year: return deserialize_text_value_int(from, output, meta);
    case column_type::bit: return deserialize_bit(from, output);
    case column_type::float_: return deserialize_text_value_float<float>(from, output);
    case column_type::double_: return deserialize_text_value_float<double>(from, output);
    case column_type::timestamp:
    case column_type::datetime: return deserialize_text_value_datetime(from, output, meta);
    case column_type::date: return deserialize_text_value_date(from, output);
    case column_type::time: return deserialize_text_value_time(from, output, meta);
    // True string types
    case column_type::char_:
    case column_type::varchar:
    case column_type::text:
    case column_type::enum_:
    case column_type::set:
    case column_type::decimal:
    case column_type::json: return deserialize_text_value_string(from, output);
    // Blobs and anything else
    case column_type::binary:
    case column_type::varbinary:
    case column_type::blob:
    case column_type::geometry:
    default: return deserialize_text_value_blob(from, output);
    }
}

#endif
