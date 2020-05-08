//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_BINARY_SERIALIZATION_IPP_
#define INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_BINARY_SERIALIZATION_IPP_

#include "boost/mysql/detail/protocol/constants.hpp"

namespace boost {
namespace mysql {
namespace detail {

// Floating point types
template <typename T>
std::enable_if_t<std::is_floating_point_v<T>>
serialize_binary_value_impl(
    T input,
    serialization_context& ctx
)
{
    // Endianness conversion
#if BOOST_ENDIAN_BIG_BYTE
    char buf [sizeof(T)];
    std::memcpy(buf, &input, sizeof(T));
    std::reverse(buf, buf + sizeof(T));
    ctx.write(buf, sizeof(T));
#else
    ctx.write(&input, sizeof(T));
#endif
}

// Time types

// Does not add the length prefix byte
inline void serialize_binary_ymd(
    const ::date::year_month_day& ymd,
    serialization_context& ctx
) noexcept
{
    serialize_fields(
        ctx,
        int2(static_cast<std::uint16_t>(static_cast<int>(ymd.year()))),
        int1(static_cast<std::uint8_t>(static_cast<unsigned>(ymd.month()))),
        int1(static_cast<std::uint8_t>(static_cast<unsigned>(ymd.day())))
    );
}

inline void serialize_binary_value_impl(
    const date& input,
    serialization_context& ctx
)
{
    ::date::year_month_day ymd (input);
    assert(ymd.ok());

    serialize(int1(binc::date_sz), ctx);
    serialize_binary_ymd(ymd, ctx);
}

inline void serialize_binary_value_impl(
    const datetime& input,
    serialization_context& ctx
)
{
    // Break datetime
    auto days = ::date::floor<::date::days>(input);
    ::date::year_month_day ymd (days);
    ::date::time_of_day<std::chrono::microseconds> tod (input - days);
    assert(ymd.ok());

    // Serialize
    serialize(int1(binc::datetime_dhmsu_sz), ctx);
    serialize_binary_ymd(ymd, ctx);
    serialize_fields(
        ctx,
        int1(static_cast<std::uint8_t>(tod.hours().count())),
        int1(static_cast<std::uint8_t>(tod.minutes().count())),
        int1(static_cast<std::uint8_t>(tod.seconds().count())),
        int4(static_cast<std::uint32_t>(tod.subseconds().count()))
    );
}

inline void serialize_binary_value_impl(
    const time& input,
    serialization_context& ctx
)
{
    // Break time
    auto days = std::chrono::duration_cast<::date::days>(input);
    auto hours = std::chrono::duration_cast<std::chrono::hours>(input % ::date::days(1));
    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(input % std::chrono::hours(1));
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(input % std::chrono::minutes(1));
    auto microseconds = input % std::chrono::seconds(1);
    int1 is_negative (input.count() < 0 ? 1 : 0);

    // Serialize
    serialize_fields(
        ctx,
        int1(binc::time_dhmsu_sz),
        is_negative,
        int4(static_cast<std::uint32_t>(std::abs(days.count()))),
        int1(static_cast<std::uint8_t>(std::abs(hours.count()))),
        int1(static_cast<std::uint8_t>(std::abs(minutes.count()))),
        int1(static_cast<std::uint8_t>(std::abs(seconds.count()))),
        int4(static_cast<std::uint32_t>(std::abs(microseconds.count())))
    );
}


struct size_visitor
{
    const serialization_context& ctx;

    size_visitor(const serialization_context& ctx) noexcept: ctx(ctx) {}

    // ints and floats
    template <typename T>
    std::size_t operator()(T) noexcept { return sizeof(T); }

    std::size_t operator()(std::string_view v) noexcept { return get_size(string_lenenc(v), ctx); }
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
    void operator()(const T& v) noexcept { serialize_binary_value_impl(v, ctx); }

    void operator()(std::int32_t v) noexcept { serialize(int4_signed(v), ctx); }
    void operator()(std::uint32_t v) noexcept { serialize(int4(v), ctx); }
    void operator()(std::int64_t v) noexcept { serialize(int8_signed(v), ctx); }
    void operator()(std::uint64_t v) noexcept { serialize(int8(v), ctx); }
    void operator()(std::string_view v) noexcept { serialize(string_lenenc(v), ctx); }
    void operator()(std::nullptr_t) noexcept {}
};

} // detail
} // mysql
} // boost


inline std::size_t boost::mysql::detail::get_binary_value_size(
    const value& input,
    const serialization_context& ctx
) noexcept
{
    return std::visit(size_visitor(ctx), input);
}

inline void boost::mysql::detail::serialize_binary_value(
    const value& input,
    serialization_context& ctx
) noexcept
{
    std::visit(serialize_visitor(ctx), input);
}



#endif
