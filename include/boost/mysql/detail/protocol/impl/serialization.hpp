//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_SERIALIZATION_HPP
#define BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_SERIALIZATION_HPP

#include <cmath>

namespace boost {
namespace mysql {
namespace detail {

// Helpers for structs
struct is_command_helper
{
    template <class T>
    static constexpr std::true_type get(decltype(T::command_id)*);

    template <class T>
    static constexpr std::false_type get(...);
};

template <class T>
struct is_command : decltype(is_command_helper::get<T>(nullptr))
{
};

struct is_struct_with_fields_helper
{
    struct test_op
    {
        template <class T>
        void operator()(const T&) {}
    };

    template <class T>
    static constexpr std::true_type get(decltype(T::apply(std::declval<T&>(), test_op()))*);

    template <class T>
    static constexpr std::false_type get(...);
};

struct struct_deserialize_op
{
    deserialization_context& ctx;
    errc result;

    struct_deserialize_op(deserialization_context& ctx) noexcept :
        ctx(ctx), result(errc::ok) {}

    template <class... Types>
    void operator()(Types&... output) noexcept
    {
        result = deserialize(ctx, output...);
    }
};

struct struct_get_size_op
{
    const serialization_context& ctx;
    std::size_t result;

    struct_get_size_op(const serialization_context& ctx, std::size_t init_value) noexcept :
        ctx(ctx), result(init_value) {}

    template <class... Types>
    void operator()(const Types&... input) noexcept
    {
        result += get_size(ctx, input...);
    }
};

struct struct_serialize_op
{
    serialization_context& ctx;

    struct_serialize_op(serialization_context& ctx) noexcept : ctx(ctx) {}

    template <class... Types>
    void operator()(const Types&... input) noexcept
    {
        serialize(ctx, input...);
    }
};

// Helpers to serialize the command id byte
template <class T>
void serialize_command_id(
    serialization_context& ctx,
    std::true_type
) noexcept
{
    constexpr std::uint8_t command_id = T::command_id;
    serialize(ctx, command_id);
}

template <class T>
void serialize_command_id(serialization_context&, std::false_type) noexcept {}

inline errc deserialize_helper(deserialization_context&) noexcept
{
    return errc::ok;
}

template <class FirstType, class... Types>
errc deserialize_helper(
    deserialization_context& ctx,
    FirstType& first,
    Types&... tail
) noexcept
{
    errc err = serialization_traits<FirstType>::deserialize_(ctx, first);
    if (err == errc::ok)
    {
        err = deserialize_helper(ctx, tail...);
    }
    return err;
}

inline void serialize_helper(serialization_context&) noexcept {}

template <class FirstType, class... Types>
void serialize_helper(
    serialization_context& ctx,
    const FirstType& first,
    const Types&... tail
)
{
    serialization_traits<FirstType>::serialize_(ctx, first);
    serialize_helper(ctx, tail...);
}

inline std::size_t get_size_helper(const serialization_context&) noexcept
{
    return 0;
}

template <class FirstType, class... Types>
std::size_t get_size_helper(
    const serialization_context& ctx,
    const FirstType& input,
    const Types&... tail
) noexcept
{
    return serialization_traits<FirstType>::get_size_(ctx, input) +
           get_size_helper(ctx, tail...);
}

} // detail
} // mysql
} // boost

// Common
template <class... Types>
boost::mysql::errc boost::mysql::detail::deserialize(
    deserialization_context& ctx,
    Types&... output
) noexcept
{
    return deserialize_helper(ctx, output...);
}

template <class... Types>
void boost::mysql::detail::serialize(
    serialization_context& ctx,
    const Types&... input
) noexcept
{
    serialize_helper(ctx, input...);
}

template <class... Types>
std::size_t boost::mysql::detail::get_size(
    const serialization_context& ctx,
    const Types&... input
) noexcept
{
    return get_size_helper(ctx, input...);
}

// Plain ints
template <class T>
boost::mysql::errc
boost::mysql::detail::serialization_traits<
    T,
    boost::mysql::detail::serialization_tag::plain_int
>::deserialize_(
    deserialization_context& ctx,
    T& output
) noexcept
{
    constexpr std::size_t sz = sizeof(T);
    if (!ctx.enough_size(sz))
    {
        return errc::incomplete_message;
    }
    output = boost::endian::endian_load<
        T, sz, boost::endian::order::little>(ctx.first());
    ctx.advance(sz);
    return errc::ok;
}

template <class T>
void boost::mysql::detail::serialization_traits<
    T,
    boost::mysql::detail::serialization_tag::plain_int
>::serialize_(
    serialization_context& ctx,
    T input
) noexcept
{
    boost::endian::endian_store<
        T, sizeof(T), boost::endian::order::little>(ctx.first(), input);
    ctx.advance(sizeof(T));
}

// Structs
template <class T>
constexpr bool boost::mysql::detail::is_struct_with_fields()
{
    return decltype(is_struct_with_fields_helper::get<T>(nullptr))::value;
}

template <class T>
boost::mysql::errc
boost::mysql::detail::serialization_traits<
    T,
    boost::mysql::detail::serialization_tag::struct_with_fields
>::deserialize_(
    deserialization_context& ctx,
    T& output
) noexcept
{
    struct_deserialize_op op (ctx);
    T::apply(output, op);
    return op.result;
}

template <class T>
void
boost::mysql::detail::serialization_traits<
    T,
    boost::mysql::detail::serialization_tag::struct_with_fields
>::serialize_(
    serialization_context& ctx,
    const T& input
) noexcept
{
    // For commands, add the command ID. Commands are only sent by the client,
    // so this is not considered in the deserialization functions.
    serialize_command_id<T>(ctx, is_command<T>());
    T::apply(input, struct_serialize_op(ctx));
}

template <class T>
std::size_t
boost::mysql::detail::serialization_traits<
    T,
    boost::mysql::detail::serialization_tag::struct_with_fields
>::get_size_(const serialization_context& ctx, const T& input) noexcept
{
    struct_get_size_op op (ctx, is_command<T>::value ? 1 : 0);
    T::apply(input, op);
    return op.result;
}

// Miscellanea
template <class Serializable, class Allocator>
void boost::mysql::detail::serialize_message(
    const Serializable& input,
    capabilities caps,
    basic_bytestring<Allocator>& buffer
)
{
    serialization_context ctx (caps);
    std::size_t size = get_size(ctx, input);
    buffer.resize(size);
    ctx.set_first(buffer.data());
    serialize(ctx, input);
    assert(ctx.first() == buffer.data() + buffer.size());
}

template <class Deserializable>
boost::mysql::error_code boost::mysql::detail::deserialize_message(
    deserialization_context& ctx,
    Deserializable& output
)
{
    auto err = deserialize(ctx, output);
    if (err != errc::ok)
        return make_error_code(err);
    if (!ctx.empty())
        return make_error_code(errc::extra_bytes);
    return error_code();
}

template <class... Types>
void boost::mysql::detail::serialize_fields(
    serialization_context& ctx,
    const Types&... fields
) noexcept
{
    serialize_fields_helper(ctx, fields...);
}

template <class T>
constexpr boost::mysql::detail::serialization_tag
boost::mysql::detail::get_serialization_tag()
{
    return
        std::is_integral<T>::value ? serialization_tag::plain_int :
        std::is_enum<T>::value ? serialization_tag::enumeration :
        is_struct_with_fields<T>() ? serialization_tag::struct_with_fields :
        serialization_tag::none;
}


#endif /* INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_SERIALIZATION_HPP_ */
