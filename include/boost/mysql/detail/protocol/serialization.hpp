#ifndef MYSQL_ASIO_IMPL_SERIALIZATION_HPP
#define MYSQL_ASIO_IMPL_SERIALIZATION_HPP

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
	floating_point,
	enumeration,
	struct_with_fields
};

template <typename T>
constexpr serialization_tag get_serialization_tag();

template <typename T, serialization_tag=get_serialization_tag<T>()>
struct serialization_traits;

template <typename T>
errc deserialize(T& output, deserialization_context& ctx) noexcept
{
	return serialization_traits<T>::deserialize_(output, ctx);
}

template <typename T>
void serialize(const T& input, serialization_context& ctx) noexcept
{
	serialization_traits<T>::serialize_(input, ctx);
}

template <typename T>
std::size_t get_size(const T& input, const serialization_context& ctx) noexcept
{
	return serialization_traits<T>::get_size_(input, ctx);
}

// Fixed-size integers
template <typename T>
constexpr bool is_fixed_size_int();

template <typename T>
struct serialization_traits<T, serialization_tag::fixed_size_int>
{
	static errc deserialize_(T& output, deserialization_context& ctx) noexcept;
	static void serialize_(T input, serialization_context& ctx) noexcept;
	static constexpr std::size_t get_size_(T, const serialization_context&) noexcept;
};

// int_lenenc
template <>
struct serialization_traits<int_lenenc, serialization_tag::none>
{
	static inline errc deserialize_(int_lenenc& output, deserialization_context& ctx) noexcept;
	static inline void serialize_(int_lenenc input, serialization_context& ctx) noexcept;
	static inline std::size_t get_size_(int_lenenc input, const serialization_context&) noexcept;
};


// string_fixed
template <std::size_t N>
struct serialization_traits<string_fixed<N>, serialization_tag::none>
{
	static errc deserialize_(string_fixed<N>& output, deserialization_context& ctx) noexcept;
	static void serialize_(const string_fixed<N>& input, serialization_context& ctx) noexcept
	{
		ctx.write(input.value.data(), N);
	}
	static constexpr std::size_t get_size_(const string_fixed<N>&, const serialization_context&) noexcept
	{
		return N;
	}
};


// string_null
template <>
struct serialization_traits<string_null, serialization_tag::none>
{
	static inline errc deserialize_(string_null& output, deserialization_context& ctx) noexcept;
	static inline void serialize_(string_null input, serialization_context& ctx) noexcept
	{
		ctx.write(input.value.data(), input.value.size());
		ctx.write(0); // null terminator
	}
	static inline std::size_t get_size_(string_null input, const serialization_context&) noexcept
	{
		return input.value.size() + 1;
	}
};

// string_eof
template <>
struct serialization_traits<string_eof, serialization_tag::none>
{
	static inline errc deserialize_(string_eof& output, deserialization_context& ctx) noexcept;
	static inline void serialize_(string_eof input, serialization_context& ctx) noexcept
	{
		ctx.write(input.value.data(), input.value.size());
	}
	static inline std::size_t get_size_(string_eof input, const serialization_context&) noexcept
	{
		return input.value.size();
	}
};

// string_lenenc
template <>
struct serialization_traits<string_lenenc, serialization_tag::none>
{
	static inline errc deserialize_(string_lenenc& output, deserialization_context& ctx) noexcept;
	static inline void serialize_(string_lenenc input, serialization_context& ctx) noexcept
	{
		serialize(int_lenenc(input.value.size()), ctx);
		ctx.write(input.value.data(), input.value.size());
	}
	static inline std::size_t get_size_(string_lenenc input, const serialization_context& ctx) noexcept
	{
		return get_size(int_lenenc(input.value.size()), ctx) + input.value.size();
	}
};

// Enums
template <typename T>
struct serialization_traits<T, serialization_tag::enumeration>
{
	using underlying_type = std::underlying_type_t<T>;
	using serializable_type = value_holder<underlying_type>;

	static errc deserialize_(T& output, deserialization_context& ctx) noexcept;
	static void serialize_(T input, serialization_context& ctx) noexcept
	{
		serialize(serializable_type(static_cast<underlying_type>(input)), ctx);
	}
	static std::size_t get_size_(T, const serialization_context&) noexcept;
};

// Floating points
template <typename T>
struct serialization_traits<T, serialization_tag::floating_point>
{
	static_assert(std::numeric_limits<T>::is_iec559);
	static errc deserialize_(T& output, deserialization_context& ctx) noexcept;
	static void serialize_(T input, serialization_context& ctx) noexcept;
	static std::size_t get_size_(T, const serialization_context&) noexcept
	{
		return sizeof(T);
	}
};


// Dates and times
template <>
struct serialization_traits<date, serialization_tag::none>
{
	static inline std::size_t get_size_(const date& input, const serialization_context& ctx) noexcept;
	static inline void serialize_(const date& input, serialization_context& ctx) noexcept;
	static inline errc deserialize_(date& output, deserialization_context& ctx) noexcept;
};

template <>
struct serialization_traits<datetime, serialization_tag::none>
{
	static inline std::size_t get_size_(const datetime& input, const serialization_context& ctx) noexcept;
	static inline void serialize_(const datetime& input, serialization_context& ctx) noexcept;
	static inline errc deserialize_(datetime& output, deserialization_context& ctx) noexcept;
};

template <>
struct serialization_traits<time, serialization_tag::none>
{
	static inline std::size_t get_size_(const time& input, const serialization_context& ctx) noexcept;
	static inline void serialize_(const time& input, serialization_context& ctx) noexcept;
	static inline errc deserialize_(time& output, deserialization_context& ctx) noexcept;
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
	static errc deserialize_(T& output, deserialization_context& ctx) noexcept;
	static void serialize_(const T& input, serialization_context& ctx) noexcept;
	static std::size_t get_size_(const T& input, const serialization_context& ctx) noexcept;
};

// Use these to make all messages implement all methods, leaving the not required
// ones with a default implementation. While this is not strictly required,
// it simplifies the testing infrastructure
template <typename T>
struct noop_serialize
{
	static inline std::size_t get_size_(const T&, const serialization_context&) noexcept { return 0; }
	static inline void serialize_(const T&, serialization_context&) noexcept {}
};

template <typename T>
struct noop_deserialize
{
	static inline errc deserialize_(T&, deserialization_context&) noexcept { return errc::ok; }
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
	Deserializable& output,
	deserialization_context& ctx
);

// Helpers for (de) serializing a set of fields
template <typename... Types>
errc deserialize_fields(deserialization_context& ctx, Types&... fields) noexcept;

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
