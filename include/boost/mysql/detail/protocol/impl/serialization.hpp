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

// Helpers for fixed size ints
template <typename T>
constexpr std::size_t get_int_size()
{
    static_assert(is_fixed_size_int<T>());
    return std::is_same<T, int3>::value ? 3 :
           std::is_same<T, int6>::value ? 6 : sizeof(T);
}

// Helpers for structs
struct is_command_helper
{
    template <typename T>
    static constexpr std::true_type get(decltype(T::command_id)*);

    template <typename T>
    static constexpr std::false_type get(...);
};

template <typename T>
struct is_command : decltype(is_command_helper::get<T>(nullptr))
{
};

template <typename T>
using struct_index_sequence = std::make_index_sequence<std::tuple_size_v<
    decltype(get_struct_fields<T>::value)
>>;

template <typename T, std::size_t... index>
errc deserialize_struct(
    deserialization_context& ctx,
    T& output,
    std::index_sequence<index...>
) noexcept
{
    return deserialize(
        ctx,
        (output.*(std::get<index>(get_struct_fields<T>::value)))...
    );
}

template <typename T, std::size_t... index>
void serialize_struct(
    serialization_context& ctx,
    const T& input,
    std::index_sequence<index...>
) noexcept
{
    serialize(
        ctx,
        (input.*(std::get<index>(get_struct_fields<T>::value)))...
    );
}

template <typename T, std::size_t... index>
std::size_t get_size_struct(
    const serialization_context& ctx,
    const T& input,
    std::index_sequence<index...>
) noexcept
{
    return get_size(
        ctx,
        (input.*(std::get<index>(get_struct_fields<T>::value)))...
    );
}

inline errc deserialize_helper(deserialization_context&) noexcept
{
    return errc::ok;
}

template <typename FirstType, typename... Types>
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

template <typename FirstType, typename... Types>
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

template <typename FirstType, typename... Types>
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
template <typename... Types>
boost::mysql::errc boost::mysql::detail::deserialize(
    deserialization_context& ctx,
    Types&... output
) noexcept
{
    return deserialize_helper(ctx, output...);
}

template <typename... Types>
void boost::mysql::detail::serialize(
    serialization_context& ctx,
    const Types&... input
) noexcept
{
    serialize_helper(ctx, input...);
}

template <typename... Types>
std::size_t boost::mysql::detail::get_size(
    const serialization_context& ctx,
    const Types&... input
) noexcept
{
    return get_size_helper(ctx, input...);
}

// Fixed size ints
template <typename T>
constexpr bool boost::mysql::detail::is_fixed_size_int()
{
    return
        std::is_integral<get_value_type_t<T>>::value &&
        std::is_base_of<value_holder<get_value_type_t<T>>, T>::value &&
        !std::is_same<T, int_lenenc>::value;
}

template <typename T>
boost::mysql::errc
boost::mysql::detail::serialization_traits<
    T,
    boost::mysql::detail::serialization_tag::fixed_size_int
>::deserialize_(
    deserialization_context& ctx,
    T& output
) noexcept
{
    static_assert(std::is_standard_layout_v<decltype(T::value)>);

    constexpr auto size = get_int_size<T>();
    if (!ctx.enough_size(size))
    {
        return errc::incomplete_message;
    }

    memset(&output.value, 0, sizeof(output.value));
    memcpy(&output.value, ctx.first(), size);
    boost::endian::little_to_native_inplace(output.value);
    ctx.advance(size);

    return errc::ok;
}

template <typename T>
void boost::mysql::detail::serialization_traits<
    T,
    boost::mysql::detail::serialization_tag::fixed_size_int
>::serialize_(
    serialization_context& ctx,
    T input
) noexcept
{
    boost::endian::native_to_little_inplace(input.value);
    ctx.write(&input.value, get_int_size<T>());
}

template <typename T>
constexpr std::size_t boost::mysql::detail::serialization_traits<
    T,
    boost::mysql::detail::serialization_tag::fixed_size_int
>::get_size_(const serialization_context&, T) noexcept
{
    return get_int_size<T>();
}

// Fixed size strings
template <std::size_t N>
boost::mysql::errc boost::mysql::detail::serialization_traits<
    boost::mysql::detail::string_fixed<N>,
    boost::mysql::detail::serialization_tag::none
>::deserialize_(
    deserialization_context& ctx,
    string_fixed<N>& output
) noexcept
{
    if (!ctx.enough_size(N))
    {
        return errc::incomplete_message;
    }
    memcpy(output.data(), ctx.first(), N);
    ctx.advance(N);
    return errc::ok;
}

// Enums
template <typename T>
boost::mysql::errc
boost::mysql::detail::serialization_traits<
    T,
    boost::mysql::detail::serialization_tag::enumeration
>::deserialize_(
    deserialization_context& ctx,
    T& output
) noexcept
{
    serializable_type value;
    errc err = deserialize(ctx, value);
    if (err != errc::ok)
    {
        return err;
    }
    output = static_cast<T>(value.value);
    return errc::ok;
}

template <typename T>
std::size_t boost::mysql::detail::serialization_traits<
    T,
    boost::mysql::detail::serialization_tag::enumeration
>::get_size_(const serialization_context&, T) noexcept
{
    return get_int_size<serializable_type>();
}

// Structs
template <typename T>
constexpr bool boost::mysql::detail::is_struct_with_fields()
{
    return !std::is_same_v<
        std::decay_t<decltype(get_struct_fields<T>::value)>,
        not_a_struct_with_fields
    >;
}

template <typename T>
boost::mysql::errc
boost::mysql::detail::serialization_traits<
    T,
    boost::mysql::detail::serialization_tag::struct_with_fields
>::deserialize_(
    deserialization_context& ctx,
    T& output
) noexcept
{
    return deserialize_struct(ctx, output, struct_index_sequence<T>());
}

template <typename T>
void
boost::mysql::detail::serialization_traits<
    T,
    boost::mysql::detail::serialization_tag::struct_with_fields
>::serialize_(
    [[maybe_unused]] serialization_context& ctx,
    [[maybe_unused]] const T& input
) noexcept
{
    // For commands, add the command ID. Commands are only sent by the client,
    // so this is not considered in the deserialization functions.
    if constexpr (is_command<T>::value)
    {
        serialize(ctx, int1(T::command_id));
    }
    serialize_struct(ctx, input, struct_index_sequence<T>());
}

template <typename T>
std::size_t
boost::mysql::detail::serialization_traits<
    T,
    boost::mysql::detail::serialization_tag::struct_with_fields
>::get_size_(const serialization_context& ctx, const T& input) noexcept
{
    std::size_t res = is_command<T>::value ? 1 : 0;
    res += get_size_struct(ctx, input, struct_index_sequence<T>());
    return res;
}

// Miscellanea
template <typename Serializable, typename Allocator>
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

template <typename Deserializable>
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

template <typename... Types>
void boost::mysql::detail::serialize_fields(
    serialization_context& ctx,
    const Types&... fields
) noexcept
{
    serialize_fields_helper(ctx, fields...);
}

template <typename T>
constexpr boost::mysql::detail::serialization_tag
boost::mysql::detail::get_serialization_tag()
{
    return
        is_fixed_size_int<T>() ? serialization_tag::fixed_size_int :
        std::is_enum<T>::value ? serialization_tag::enumeration :
        is_struct_with_fields<T>() ? serialization_tag::struct_with_fields :
        serialization_tag::none;
}


#endif /* INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_SERIALIZATION_HPP_ */
