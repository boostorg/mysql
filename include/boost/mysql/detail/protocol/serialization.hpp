#ifndef MYSQL_ASIO_IMPL_SERIALIZATION_HPP
#define MYSQL_ASIO_IMPL_SERIALIZATION_HPP

#include <boost/endian/conversion.hpp>
#include <type_traits>
#include <algorithm>
#include "boost/mysql/detail/protocol/protocol_types.hpp"
#include "boost/mysql/detail/protocol/serialization_context.hpp"
#include "boost/mysql/detail/protocol/deserialization_context.hpp"
#include "boost/mysql/detail/auxiliar/get_struct_fields.hpp"
#include "boost/mysql/detail/auxiliar/bytestring.hpp"
#include "boost/mysql/value.hpp"
#include "boost/mysql/error.hpp"

namespace boost {
namespace mysql {
namespace detail {

struct no_serialization_tag {};

template <typename T>
struct get_serialization_tag;

template <typename T, typename Tag=typename get_serialization_tag<T>::type>
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
struct fixed_size_int_tag {};

template <typename T>
constexpr bool is_fixed_size_int();

template <typename T>
struct serialization_traits<T, fixed_size_int_tag>
{
	static errc deserialize_(T& output, deserialization_context& ctx) noexcept;
	static void serialize_(T input, serialization_context& ctx) noexcept;
	static constexpr std::size_t get_size_(T, const serialization_context&) noexcept;
};

// int_lenenc
template <>
struct serialization_traits<int_lenenc, no_serialization_tag>
{
	static inline errc deserialize_(int_lenenc& output, deserialization_context& ctx) noexcept;
	static inline void serialize_(int_lenenc input, serialization_context& ctx) noexcept;
	static inline std::size_t get_size_(int_lenenc input, const serialization_context&) noexcept;
};


// string_fixed
template <std::size_t N>
struct serialization_traits<string_fixed<N>, no_serialization_tag>
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
struct serialization_traits<string_null, no_serialization_tag>
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
struct serialization_traits<string_eof, no_serialization_tag>
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
struct serialization_traits<string_lenenc, no_serialization_tag>
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
struct enum_tag {};

template <typename T>
struct serialization_traits<T, enum_tag>
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
struct floating_point_tag {};

template <typename T>
struct serialization_traits<T, floating_point_tag>
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
struct serialization_traits<date, no_serialization_tag>
{
	static inline std::size_t get_size_(const date& input, const serialization_context& ctx) noexcept;
	static inline void serialize_(const date& input, serialization_context& ctx) noexcept;
	static inline errc deserialize_(date& output, deserialization_context& ctx) noexcept;
};

template <>
struct serialization_traits<datetime, no_serialization_tag>
{
	static inline std::size_t get_size_(const datetime& input, const serialization_context& ctx) noexcept;
	static inline void serialize_(const datetime& input, serialization_context& ctx) noexcept;
	static inline errc deserialize_(datetime& output, deserialization_context& ctx) noexcept;
};

template <>
struct serialization_traits<time, no_serialization_tag>
{
	static inline std::size_t get_size_(const time& input, const serialization_context& ctx) noexcept;
	static inline void serialize_(const time& input, serialization_context& ctx) noexcept;
	static inline errc deserialize_(time& output, deserialization_context& ctx) noexcept;
};

// Structs and commands (messages)
struct struct_tag {};

template <typename T>
constexpr bool is_struct_with_fields()
{
	return !std::is_same_v<
		std::decay_t<decltype(get_struct_fields<T>::value)>,
		not_a_struct_with_fields
	>;
}




template <typename T>
struct serialization_traits<T, struct_tag>
{
	static errc deserialize_(T& output, deserialization_context& ctx) noexcept;
	static void serialize_(const T& input, serialization_context& ctx) noexcept;
	static std::size_t get_size_(const T& input, const serialization_context& ctx) noexcept;
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

// Dummy type to indicate no (de)serialization is required
struct dummy_serializable
{
	explicit dummy_serializable(...) {} // Make it constructible from anything
};

/*struct noop_serialization_traits
{
	static inline std::size_t get_size_(dummy_serializable, const serialization_context&) noexcept { return 0; }
	static inline void serialize_(dummy_serializable, serialization_context&) noexcept {}
	static inline errc deserialize_(dummy_serializable, deserialization_context&) noexcept { return errc::ok; }
};*/

struct noop_serialization_traits
{
	static inline std::size_t get_size_(...) noexcept { return 0; }
	static inline void serialize_(...) noexcept {}
	static inline errc deserialize_(...) noexcept { return errc::ok; }
};

template <>
struct serialization_traits<dummy_serializable, no_serialization_tag> : noop_serialization_traits
{
};


// Helpers for (de) serializing a set of fields
template <typename FirstType>
errc deserialize_fields(deserialization_context& ctx, FirstType& field) noexcept
{
	return deserialize(field, ctx);
}

template <typename FirstType, typename... Types>
errc deserialize_fields(deserialization_context& ctx, FirstType& field, Types&... fields_tail) noexcept
{
	errc err = deserialize(field, ctx);
	if (err == errc::ok)
	{
		err = deserialize_fields(ctx, fields_tail...);
	}
	return err;
}

template <typename FirstType>
void serialize_fields(serialization_context& ctx, const FirstType& field) noexcept
{
	serialize(field, ctx);
}

template <typename FirstType, typename... Types>
void serialize_fields(serialization_context& ctx, const FirstType& field, const Types&... fields_tail)
{
	serialize(field, ctx);
	serialize_fields(ctx, fields_tail...);
}

inline std::pair<error_code, std::uint8_t> deserialize_message_type(
	deserialization_context& ctx
);

template <typename T>
struct get_serialization_tag : std::conditional<
	is_fixed_size_int<T>(),
	fixed_size_int_tag,
	std::conditional_t<
		std::is_floating_point<T>::value,
		floating_point_tag,
		std::conditional_t<
			std::is_enum<T>::value,
			enum_tag,
			std::conditional_t<
				is_struct_with_fields<T>(),
				struct_tag,
				no_serialization_tag
			>
		>
	>
> {};

} // detail
} // mysql
} // boost

#include "boost/mysql/detail/protocol/impl/serialization.hpp"
#include "boost/mysql/detail/protocol/impl/serialization.ipp"

#endif
