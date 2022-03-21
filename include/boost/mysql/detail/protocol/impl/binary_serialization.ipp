//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_BINARY_SERIALIZATION_IPP
#define BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_BINARY_SERIALIZATION_IPP

#pragma once

#include <boost/mysql/detail/protocol/binary_serialization.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>
#include <boost/mysql/detail/protocol/date.hpp>

namespace boost {
namespace mysql {
namespace detail {

// Floating point types
template <class T>
typename std::enable_if<std::is_floating_point<T>::value>::type
serialize_binary_value_impl(
    serialization_context& ctx,
    T input
)
{
    boost::endian::endian_store<T, sizeof(T),
        boost::endian::order::little>(ctx.first(), input);
    ctx.advance(sizeof(T));
}

// Time types

// Does not add the length prefix byte
inline void serialize_binary_ymd(
    serialization_context& ctx,
    const year_month_day& ymd
) noexcept
{
    assert(ymd.years >= 0 && ymd.years <= 0xffff);
    serialize(
        ctx,
        static_cast<std::uint16_t>(ymd.years),
        static_cast<std::uint8_t>(ymd.month),
        static_cast<std::uint8_t>(ymd.day)
    );
}

inline void serialize_binary_value_impl(
    serialization_context& ctx,
    const date& input
)
{
    assert(input >= min_date && input <= max_date);
    auto ymd = days_to_ymd(input.time_since_epoch().count());

    serialize(ctx, static_cast<std::uint8_t>(binc::date_sz));
    serialize_binary_ymd(ctx, ymd);
}

inline void serialize_binary_value_impl(
    serialization_context& ctx,
    const datetime& input
)
{
    assert(input >= min_datetime && input <= max_datetime);

    // Break datetime
    using namespace std::chrono;
    auto input_dur = input.time_since_epoch();
    auto num_micros = duration_cast<microseconds>(input_dur % seconds(1));
    auto num_secs = duration_cast<seconds>(input_dur % minutes(1) - num_micros);
    auto num_mins = duration_cast<minutes>(input_dur % hours(1) - num_secs);
    auto num_hours = duration_cast<hours>(input_dur % days(1) - num_mins);
    auto num_days = duration_cast<days>(input_dur - num_hours);

    // Serialize
    serialize(ctx, static_cast<std::uint8_t>(binc::datetime_dhmsu_sz));
    serialize_binary_ymd(ctx, days_to_ymd(num_days.count()));
    serialize(
        ctx,
        static_cast<std::uint8_t>(num_hours.count()),
        static_cast<std::uint8_t>(num_mins.count()),
        static_cast<std::uint8_t>(num_secs.count()),
        static_cast<std::uint32_t>(num_micros.count())
    );
}

inline void serialize_binary_value_impl(
    serialization_context& ctx,
    const time& input
)
{
    // Break time
    using namespace std::chrono;
    auto num_micros = duration_cast<microseconds>(input % seconds(1));
    auto num_secs = duration_cast<seconds>(input % minutes(1) - num_micros);
    auto num_mins = duration_cast<minutes>(input % hours(1) - num_secs);
    auto num_hours = duration_cast<hours>(input % days(1) - num_mins);
    auto num_days = duration_cast<days>(input - num_hours);
    std::uint8_t is_negative = (input.count() < 0) ? 1 : 0;

    // Serialize
    serialize(
        ctx,
        static_cast<std::uint8_t>(binc::time_dhmsu_sz),
        is_negative,
        static_cast<std::uint32_t>(std::abs(num_days.count())),
        static_cast<std::uint8_t>(std::abs(num_hours.count())),
        static_cast<std::uint8_t>(std::abs(num_mins.count())),
        static_cast<std::uint8_t>(std::abs(num_secs.count())),
        static_cast<std::uint32_t>(std::abs(num_micros.count()))
    );
}


struct size_visitor
{
    const serialization_context& ctx;

    size_visitor(const serialization_context& ctx) noexcept: ctx(ctx) {}

    // ints and floats
    template <class T>
    std::size_t operator()(T) noexcept { return sizeof(T); }

    std::size_t operator()(boost::string_view v) noexcept { return get_size(ctx, string_lenenc(v)); }
    std::size_t operator()(const date&) noexcept { return binc::date_sz + binc::length_sz; }
    std::size_t operator()(const datetime&) noexcept { return binc::datetime_dhmsu_sz + binc::length_sz; }
    std::size_t operator()(const time&) noexcept { return binc::time_dhmsu_sz + binc::length_sz; }
    std::size_t operator()(null_t) noexcept { return 0; }
};

struct serialize_visitor
{
    serialization_context& ctx;

    serialize_visitor(serialization_context& ctx) noexcept: ctx(ctx) {}

    template <class T>
    void operator()(const T& v) noexcept { serialize_binary_value_impl(ctx, v); }

    void operator()(std::int64_t v) noexcept { serialize(ctx, v); }
    void operator()(std::uint64_t v) noexcept { serialize(ctx, v); }
    void operator()(boost::string_view v) noexcept { serialize(ctx, string_lenenc(v)); }
    void operator()(null_t) noexcept {}
};

} // detail
} // mysql
} // boost

inline std::size_t
boost::mysql::detail::serialization_traits<
    boost::mysql::value,
    boost::mysql::detail::serialization_tag::none
>::get_size_(
    const serialization_context& ctx,
    const value& input
) noexcept
{
    return boost::variant2::visit(size_visitor(ctx), input.to_variant());
}

inline void
boost::mysql::detail::serialization_traits<
    boost::mysql::value,
    boost::mysql::detail::serialization_tag::none
>::serialize_(
    serialization_context& ctx,
    const value& input
) noexcept
{
    boost::variant2::visit(serialize_visitor(ctx), input.to_variant());
}



#endif
