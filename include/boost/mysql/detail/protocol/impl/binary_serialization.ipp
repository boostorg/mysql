//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_BINARY_SERIALIZATION_IPP
#define BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_BINARY_SERIALIZATION_IPP

#include "boost/mysql/detail/protocol/constants.hpp"

namespace boost {
namespace mysql {
namespace detail {

// Floating point types
template <typename T>
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
    const ::date::year_month_day& ymd
) noexcept
{
    serialize(
        ctx,
        static_cast<std::uint16_t>(static_cast<int>(ymd.year())),
        static_cast<std::uint8_t>(static_cast<unsigned>(ymd.month())),
        static_cast<std::uint8_t>(static_cast<unsigned>(ymd.day()))
    );
}

inline void serialize_binary_value_impl(
    serialization_context& ctx,
    const date& input
)
{
    ::date::year_month_day ymd (input);
    assert(ymd.ok());

    serialize(ctx, static_cast<std::uint8_t>(binc::date_sz));
    serialize_binary_ymd(ctx, ymd);
}

inline void serialize_binary_value_impl(
    serialization_context& ctx,
    const datetime& input
)
{
    // Break datetime
    auto days = ::date::floor<::date::days>(input);
    ::date::year_month_day ymd (days);
    ::date::time_of_day<std::chrono::microseconds> tod (input - days);
    assert(ymd.ok());

    // Serialize
    serialize(ctx, static_cast<std::uint8_t>(binc::datetime_dhmsu_sz));
    serialize_binary_ymd(ctx, ymd);
    serialize(
        ctx,
        static_cast<std::uint8_t>(tod.hours().count()),
        static_cast<std::uint8_t>(tod.minutes().count()),
        static_cast<std::uint8_t>(tod.seconds().count()),
        static_cast<std::uint32_t>(tod.subseconds().count())
    );
}

inline void serialize_binary_value_impl(
    serialization_context& ctx,
    const time& input
)
{
    // Break time
    auto days = std::chrono::duration_cast<::date::days>(input);
    auto hours = std::chrono::duration_cast<std::chrono::hours>(input % ::date::days(1));
    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(input % std::chrono::hours(1));
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(input % std::chrono::minutes(1));
    auto microseconds = input % std::chrono::seconds(1);
    std::uint8_t is_negative = (input.count() < 0) ? 1 : 0;

    // Serialize
    serialize(
        ctx,
        static_cast<std::uint8_t>(binc::time_dhmsu_sz),
        is_negative,
        static_cast<std::uint32_t>(std::abs(days.count())),
        static_cast<std::uint8_t>(std::abs(hours.count())),
        static_cast<std::uint8_t>(std::abs(minutes.count())),
        static_cast<std::uint8_t>(std::abs(seconds.count())),
        static_cast<std::uint32_t>(std::abs(microseconds.count()))
    );
}


struct size_visitor
{
    const serialization_context& ctx;

    size_visitor(const serialization_context& ctx) noexcept: ctx(ctx) {}

    // ints and floats
    template <typename T>
    std::size_t operator()(T) noexcept { return sizeof(T); }

    std::size_t operator()(boost::string_view v) noexcept { return get_size(ctx, string_lenenc(v)); }
    std::size_t operator()(const date&) noexcept { return binc::date_sz + binc::length_sz; }
    std::size_t operator()(const datetime&) noexcept { return binc::datetime_dhmsu_sz + binc::length_sz; }
    std::size_t operator()(const time&) noexcept { return binc::time_dhmsu_sz + binc::length_sz; }
    std::size_t operator()(std::nullptr_t) noexcept { return 0; }
};

struct serialize_visitor
{
    serialization_context& ctx;

    serialize_visitor(serialization_context& ctx) noexcept: ctx(ctx) {}

    template <typename T>
    void operator()(const T& v) noexcept { serialize_binary_value_impl(ctx, v); }

    void operator()(std::int64_t v) noexcept { serialize(ctx, v); }
    void operator()(std::uint64_t v) noexcept { serialize(ctx, v); }
    void operator()(boost::string_view v) noexcept { serialize(ctx, string_lenenc(v)); }
    void operator()(std::nullptr_t) noexcept {}
};

} // detail
} // mysql
} // boost


inline std::size_t boost::mysql::detail::get_binary_value_size(
    const serialization_context& ctx,
    const value& input
) noexcept
{
    return boost::variant2::visit(size_visitor(ctx), input.to_variant());
}

inline void boost::mysql::detail::serialize_binary_value(
    serialization_context& ctx,
    const value& input
) noexcept
{
    boost::variant2::visit(serialize_visitor(ctx), input.to_variant());
}



#endif
