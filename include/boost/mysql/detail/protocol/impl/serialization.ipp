//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_SERIALIZATION_IPP
#define BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_SERIALIZATION_IPP

#include <limits>

namespace boost {
namespace mysql {
namespace detail {

inline std::string_view get_string(
    const std::uint8_t* from,
    std::size_t size
)
{
    return std::string_view(reinterpret_cast<const char*>(from), size);
}

} // detail
} // mysql
} // boost

inline boost::mysql::errc
boost::mysql::detail::serialization_traits<
    boost::mysql::detail::int_lenenc,
    boost::mysql::detail::serialization_tag::none
>::deserialize_(
    int_lenenc& output,
    deserialization_context& ctx
) noexcept
{
    int1 first_byte;
    errc err = deserialize(first_byte, ctx);
    if (err != errc::ok)
    {
        return err;
    }

    if (first_byte.value == 0xFC)
    {
        int2 value;
        err = deserialize(value, ctx);
        output.value = value.value;
    }
    else if (first_byte.value == 0xFD)
    {
        int3 value;
        err = deserialize(value, ctx);
        output.value = value.value;
    }
    else if (first_byte.value == 0xFE)
    {
        int8 value;
        err = deserialize(value, ctx);
        output.value = value.value;
    }
    else
    {
        err = errc::ok;
        output.value = first_byte.value;
    }
    return err;
}


inline void
boost::mysql::detail::serialization_traits<
    boost::mysql::detail::int_lenenc,
    boost::mysql::detail::serialization_tag::none
>::serialize_(
    int_lenenc input,
    serialization_context& ctx
) noexcept
{
    if (input.value < 251)
    {
        serialize(int1(static_cast<std::uint8_t>(input.value)), ctx);
    }
    else if (input.value < 0x10000)
    {
        ctx.write(0xfc);
        serialize(int2(static_cast<std::uint16_t>(input.value)), ctx);
    }
    else if (input.value < 0x1000000)
    {
        ctx.write(0xfd);
        serialize(int3(static_cast<std::uint32_t>(input.value)), ctx);
    }
    else
    {
        ctx.write(0xfe);
        serialize(int8(static_cast<std::uint64_t>(input.value)), ctx);
    }
}

inline std::size_t
boost::mysql::detail::serialization_traits<
    boost::mysql::detail::int_lenenc,
    boost::mysql::detail::serialization_tag::none
>::get_size_(
    int_lenenc input,
    const serialization_context&
) noexcept
{
    if (input.value < 251)
        return 1;
    else if (input.value < 0x10000)
        return 3;
    else if (input.value < 0x1000000)
        return 4;
    else
        return 9;
}

inline boost::mysql::errc
boost::mysql::detail::serialization_traits<
    boost::mysql::detail::string_null,
    boost::mysql::detail::serialization_tag::none
>::deserialize_(
    string_null& output,
    deserialization_context& ctx
) noexcept
{
    auto string_end = std::find(ctx.first(), ctx.last(), 0);
    if (string_end == ctx.last())
    {
        return errc::incomplete_message;
    }
    output.value = get_string(ctx.first(), string_end-ctx.first());
    ctx.set_first(string_end + 1); // skip the null terminator
    return errc::ok;
}

inline boost::mysql::errc
boost::mysql::detail::serialization_traits<
    boost::mysql::detail::string_eof,
    boost::mysql::detail::serialization_tag::none
>::deserialize_(
    string_eof& output,
    deserialization_context& ctx
) noexcept
{
    output.value = get_string(ctx.first(), ctx.last()-ctx.first());
    ctx.set_first(ctx.last());
    return errc::ok;
}

inline boost::mysql::errc
boost::mysql::detail::serialization_traits<
    boost::mysql::detail::string_lenenc,
    boost::mysql::detail::serialization_tag::none
>::deserialize_(
    string_lenenc& output,
    deserialization_context& ctx
) noexcept
{
    int_lenenc length;
    errc err = deserialize(length, ctx);
    if (err != errc::ok)
    {
        return err;
    }
    if (length.value > std::numeric_limits<std::size_t>::max())
    {
        return errc::protocol_value_error;
    }
    auto len = static_cast<std::size_t>(length.value);
    if (!ctx.enough_size(len))
    {
        return errc::incomplete_message;
    }

    output.value = get_string(ctx.first(), len);
    ctx.advance(len);
    return errc::ok;
}

inline std::pair<boost::mysql::error_code, std::uint8_t>
boost::mysql::detail::deserialize_message_type(
    deserialization_context& ctx
)
{
    int1 msg_type;
    std::pair<mysql::error_code, std::uint8_t> res {};
    auto err = deserialize(msg_type, ctx);
    if (err == errc::ok)
    {
        res.second = msg_type.value;
    }
    else
    {
        res.first = make_error_code(err);
    }
    return res;
}


#endif /* INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_SERIALIZATION_IPP_ */
