//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PROTOCOL_SERIALIZATION_HPP
#define BOOST_MYSQL_DETAIL_PROTOCOL_SERIALIZATION_HPP

#include <boost/endian/conversion.hpp>
#include <type_traits>
#include <algorithm>
#include "boost/mysql/detail/protocol/protocol_types.hpp"
#include "boost/mysql/detail/protocol/serialization_context.hpp"
#include "boost/mysql/detail/protocol/deserialization_context.hpp"
#include "boost/mysql/detail/auxiliar/bytestring.hpp"
#include "boost/mysql/value.hpp"
#include "boost/mysql/error.hpp"

namespace boost {
namespace mysql {
namespace detail {

enum class serialization_tag
{
    none,
    fixed_size_int,
    enumeration,
    struct_with_fields
};

template <typename T>
constexpr serialization_tag get_serialization_tag();

template <typename T, serialization_tag=get_serialization_tag<T>()>
struct serialization_traits;

template <typename... Types>
errc deserialize(deserialization_context& ctx, Types&... output) noexcept;

template <typename... Types>
void serialize(serialization_context& ctx, const Types&... input) noexcept;

template <typename... Types>
std::size_t get_size(const serialization_context& ctx, const Types&... input) noexcept;

// Fixed-size integers
template <typename T>
constexpr bool is_fixed_size_int();

template <typename T>
struct serialization_traits<T, serialization_tag::fixed_size_int>
{
    static errc deserialize_(deserialization_context& ctx, T& output) noexcept;
    static void serialize_(serialization_context& ctx, T input) noexcept;
    static constexpr std::size_t get_size_(const serialization_context&, T) noexcept;
};

// int_lenenc
template <>
struct serialization_traits<int_lenenc, serialization_tag::none>
{
    static inline errc deserialize_(deserialization_context& ctx, int_lenenc& output) noexcept;
    static inline void serialize_(serialization_context& ctx, int_lenenc input) noexcept;
    static inline std::size_t get_size_(const serialization_context&, int_lenenc input) noexcept;
};


// string_fixed
template <std::size_t N>
struct serialization_traits<string_fixed<N>, serialization_tag::none>
{
    static errc deserialize_(deserialization_context& ctx, string_fixed<N>& output) noexcept;
    static void serialize_(serialization_context& ctx, const string_fixed<N>& input) noexcept
    {
        ctx.write(input.data(), N);
    }
    static constexpr std::size_t get_size_(const serialization_context&, const string_fixed<N>&) noexcept
    {
        return N;
    }
};


// string_null
template <>
struct serialization_traits<string_null, serialization_tag::none>
{
    static inline errc deserialize_(deserialization_context& ctx, string_null& output) noexcept;
    static inline void serialize_(serialization_context& ctx, string_null input) noexcept
    {
        ctx.write(input.value.data(), input.value.size());
        ctx.write(0); // null terminator
    }
    static inline std::size_t get_size_(const serialization_context&, string_null input) noexcept
    {
        return input.value.size() + 1;
    }
};

// string_eof
template <>
struct serialization_traits<string_eof, serialization_tag::none>
{
    static inline errc deserialize_(deserialization_context& ctx, string_eof& output) noexcept;
    static inline void serialize_(serialization_context& ctx, string_eof input) noexcept
    {
        ctx.write(input.value.data(), input.value.size());
    }
    static inline std::size_t get_size_(const serialization_context&, string_eof input) noexcept
    {
        return input.value.size();
    }
};

// string_lenenc
template <>
struct serialization_traits<string_lenenc, serialization_tag::none>
{
    static inline errc deserialize_(deserialization_context& ctx, string_lenenc& output) noexcept;
    static inline void serialize_(serialization_context& ctx, string_lenenc input) noexcept
    {
        serialize(ctx, int_lenenc(input.value.size()));
        ctx.write(input.value.data(), input.value.size());
    }
    static inline std::size_t get_size_(const serialization_context& ctx, string_lenenc input) noexcept
    {
        return get_size(ctx, int_lenenc(input.value.size())) + input.value.size();
    }
};

// Enums
template <typename T>
struct serialization_traits<T, serialization_tag::enumeration>
{
    using underlying_type = std::underlying_type_t<T>;
    using serializable_type = value_holder<underlying_type>;

    static errc deserialize_(deserialization_context& ctx, T& output) noexcept;
    static void serialize_(serialization_context& ctx, T input) noexcept
    {
        serialize(ctx, serializable_type(static_cast<underlying_type>(input)));
    }
    static std::size_t get_size_(const serialization_context&, T) noexcept;
};

// Structs and commands (messages)
// To allow a limited way of reflection, structs should
// specialize get_struct_fields with a tuple of pointers to members,
// thus defining which fields should be (de)serialized in the struct
// and in which order
struct not_a_struct_with_fields {}; // Tag indicating a type is not a struct with fields

template <typename T>
struct get_struct_fields
{
    static constexpr not_a_struct_with_fields value {};
};

template <typename T>
constexpr bool is_struct_with_fields();

template <typename T>
struct serialization_traits<T, serialization_tag::struct_with_fields>
{
    static errc deserialize_(deserialization_context& ctx, T& output) noexcept;
    static void serialize_(serialization_context& ctx, const T& input) noexcept;
    static std::size_t get_size_(const serialization_context& ctx, const T& input) noexcept;
};

// Use these to make all messages implement all methods, leaving the not required
// ones with a default implementation. While this is not strictly required,
// it simplifies the testing infrastructure
template <typename T>
struct noop_serialize
{
    static inline std::size_t get_size_(const serialization_context&, const T&) noexcept { return 0; }
    static inline void serialize_(serialization_context&, const T&) noexcept {}
};

template <typename T>
struct noop_deserialize
{
    static inline errc deserialize_(deserialization_context&, T&) noexcept { return errc::ok; }
};

// Helper to serialize top-level messages
template <typename Serializable, typename Allocator>
void serialize_message(
    const Serializable& input,
    capabilities caps,
    basic_bytestring<Allocator>& buffer
);

template <typename Deserializable>
error_code deserialize_message(
    deserialization_context& ctx,
    Deserializable& output
);

// Helpers for (de) serializing a set of fields
template <typename... Types>
void serialize_fields(serialization_context& ctx, const Types&... fields) noexcept;

inline std::pair<error_code, std::uint8_t> deserialize_message_type(
    deserialization_context& ctx
);


} // detail
} // mysql
} // boost

#include "boost/mysql/detail/protocol/impl/serialization.hpp"
#include "boost/mysql/detail/protocol/impl/serialization.ipp"

#endif
