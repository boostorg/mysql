#ifndef MYSQL_ASIO_IMPL_SERIALIZATION_HPP
#define MYSQL_ASIO_IMPL_SERIALIZATION_HPP

#include <boost/endian/conversion.hpp>
#include <type_traits>
#include <algorithm>
#include "boost/mysql/detail/protocol/protocol_types.hpp"
#include "boost/mysql/detail/protocol/serialization_context.hpp"
#include "boost/mysql/detail/protocol/deserialization_context.hpp"
#include "boost/mysql/detail/aux/get_struct_fields.hpp"
#include "boost/mysql/detail/aux/bytestring.hpp"
#include "boost/mysql/value.hpp"
#include "boost/mysql/error.hpp"

namespace boost {
namespace mysql {
namespace detail {


/**
 * Base forms:
 *  errc deserialize(T& output, deserialization_context&) noexcept
 *  void  serialize(const T& input, SerializationContext&) noexcept
 *  std::size_t get_size(const T& input, const SerializationContext&) noexcept
 */

// Fixed-size integers
template <typename T>
struct is_fixed_size_int
{
	static constexpr bool value =
		std::is_integral<get_value_type_t<T>>::value &&
		std::is_base_of<value_holder<get_value_type_t<T>>, T>::value;
};

template <> struct is_fixed_size_int<int_lenenc> : std::false_type {};

template <typename T> constexpr bool is_fixed_size_v = is_fixed_size_int<T>::value;

template <typename T>
struct get_int_size
{
	static_assert(is_fixed_size_v<T>);
	static constexpr std::size_t value = sizeof(T::value);
};

template <> struct get_int_size<int3> { static constexpr std::size_t value = 3; };
template <> struct get_int_size<int6> { static constexpr std::size_t value = 6; };

template <typename T>
std::enable_if_t<is_fixed_size_int<T>::value, errc>
deserialize(T& output, deserialization_context& ctx) noexcept
{
	static_assert(std::is_standard_layout_v<decltype(T::value)>);

	constexpr auto size = get_int_size<T>::value;
	if (!ctx.enough_size(size))
	{
		return errc::incomplete_message;
	}

	memset(&output.value, 0, sizeof(output.value));
	memcpy(&output.value, ctx.first(), size);
	boost::endian::little_to_native_inplace(output);
	ctx.advance(size);

	return errc::ok;
}

template <typename T>
std::enable_if_t<is_fixed_size_v<T>>
serialize(T input, serialization_context& ctx) noexcept
{
	boost::endian::native_to_little_inplace(input);
	ctx.write(&input.value, get_int_size<T>::value);
}

template <typename T>
constexpr std::enable_if_t<is_fixed_size_v<T>, std::size_t>
get_size(T, const serialization_context&) noexcept
{
	return get_int_size<T>::value;
}

// int_lenenc
inline errc deserialize(int_lenenc& output, deserialization_context& ctx) noexcept;
inline void serialize(int_lenenc input, serialization_context& ctx) noexcept;
inline std::size_t get_size(int_lenenc input, const serialization_context&) noexcept;

// Helper for strings
inline std::string_view get_string(const std::uint8_t* from, std::size_t size)
{
	return std::string_view (reinterpret_cast<const char*>(from), size);
}

// string_fixed
template <std::size_t N>
errc deserialize(string_fixed<N>& output, deserialization_context& ctx) noexcept
{
	if (!ctx.enough_size(N))
	{
		return errc::incomplete_message;
	}
	memcpy(&output.value, ctx.first(), N);
	ctx.advance(N);
	return errc::ok;
}

template <std::size_t N>
void serialize(const string_fixed<N>& input, serialization_context& ctx) noexcept
{
	ctx.write(input.value.data(), N);
}

template <std::size_t N>
constexpr std::size_t get_size(const string_fixed<N>&, const serialization_context&) noexcept
{
	return N;
}

// string_null
inline errc deserialize(string_null& output, deserialization_context& ctx) noexcept;
inline void serialize(string_null input, serialization_context& ctx) noexcept
{
	ctx.write(input.value.data(), input.value.size());
	ctx.write(0); // null terminator
}
inline std::size_t get_size(string_null input, const serialization_context&) noexcept
{
	return input.value.size() + 1;
}

// string_eof
inline errc deserialize(string_eof& output, deserialization_context& ctx) noexcept;
inline void serialize(string_eof input, serialization_context& ctx) noexcept
{
	ctx.write(input.value.data(), input.value.size());
}
inline std::size_t get_size(string_eof input, const serialization_context&) noexcept
{
	return input.value.size();
}

// string_lenenc
inline errc deserialize(string_lenenc& output, deserialization_context& ctx) noexcept;
inline void serialize(string_lenenc input, serialization_context& ctx) noexcept
{
	serialize(int_lenenc(input.value.size()), ctx);
	ctx.write(input.value.data(), input.value.size());
}
inline std::size_t get_size(string_lenenc input, const serialization_context& ctx) noexcept
{
	return get_size(int_lenenc(input.value.size()), ctx) + input.value.size();
}

// Enums
template <typename T, typename=std::enable_if_t<std::is_enum_v<T>>>
errc deserialize(T& output, deserialization_context& ctx) noexcept
{
	value_holder<std::underlying_type_t<T>> value;
	errc err = deserialize(value, ctx);
	if (err != errc::ok)
	{
		return err;
	}
	output = static_cast<T>(value.value);
	return errc::ok;
}

template <typename T, typename=std::enable_if_t<std::is_enum_v<T>>>
void serialize(T input, serialization_context& ctx) noexcept
{
	value_holder<std::underlying_type_t<T>> value {static_cast<std::underlying_type_t<T>>(input)};
	serialize(value, ctx);
}

template <typename T, typename=std::enable_if_t<std::is_enum_v<T>>>
std::size_t get_size(T, const serialization_context&) noexcept
{
	return get_int_size<value_holder<std::underlying_type_t<T>>>::value;
}

// Floating points
static_assert(std::numeric_limits<float>::is_iec559);
static_assert(std::numeric_limits<double>::is_iec559);

template <typename T>
std::enable_if_t<std::is_floating_point_v<T>, errc>
deserialize(T& output, deserialization_context& ctx) noexcept
{
	// Size check
	if (!ctx.enough_size(sizeof(T))) return errc::incomplete_message;

	// Endianness conversion
	// Boost.Endian support for floats start at 1.71. TODO: maybe update requirements and CI
#if BOOST_ENDIAN_BIG_BYTE
	char buf [sizeof(T)];
	std::memcpy(buf, ctx.first(), sizeof(T));
	std::reverse(buf, buf + sizeof(T));
	std::memcpy(&output, buf, sizeof(T));
#else
	std::memcpy(&output, ctx.first(), sizeof(T));
#endif
	ctx.advance(sizeof(T));
	return errc::ok;
}

template <typename T>
std::enable_if_t<std::is_floating_point_v<T>>
serialize(T input, serialization_context& ctx) noexcept
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

template <typename T>
std::enable_if_t<std::is_floating_point_v<T>, std::size_t>
get_size(T, const serialization_context&) noexcept
{
	return sizeof(T);
}

// Dates and times
inline std::size_t get_size(const date& input, const serialization_context& ctx) noexcept;
inline void serialize(const date& input, serialization_context& ctx) noexcept;
inline errc deserialize(date& output, deserialization_context& ctx) noexcept;

inline std::size_t get_size(const datetime& input, const serialization_context& ctx) noexcept;
inline void serialize(const datetime& input, serialization_context& ctx) noexcept;
inline errc deserialize(datetime& output, deserialization_context& ctx) noexcept;

inline std::size_t get_size(const time& input, const serialization_context& ctx) noexcept;
inline void serialize(const time& input, serialization_context& ctx) noexcept;
inline errc deserialize(time& output, deserialization_context& ctx) noexcept;

// Structs and commands (messages)
template <typename T>
struct is_struct_with_fields
{
	static constexpr bool value = !std::is_same_v<
		std::decay_t<decltype(get_struct_fields<T>::value)>,
		not_a_struct_with_fields
	>;
};

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

template <std::size_t index, typename T>
errc deserialize_struct(
	[[maybe_unused]] T& output,
	[[maybe_unused]] deserialization_context& ctx
) noexcept
{
	constexpr auto fields = get_struct_fields<T>::value;
	if constexpr (index == std::tuple_size<decltype(fields)>::value)
	{
		return errc::ok;
	}
	else
	{
		constexpr auto pmem = std::get<index>(fields);
		errc err = deserialize(output.*pmem, ctx);
		if (err != errc::ok)
		{
			return err;
		}
		else
		{
			return deserialize_struct<index+1>(output, ctx);
		}
	}
}

template <typename T>
std::enable_if_t<is_struct_with_fields<T>::value, errc>
deserialize(T& output, deserialization_context& ctx) noexcept
{
	return deserialize_struct<0>(output, ctx);
}

template <std::size_t index, typename T>
void serialize_struct(
	[[maybe_unused]] const T& value,
	[[maybe_unused]] serialization_context& ctx
) noexcept
{
	constexpr auto fields = get_struct_fields<T>::value;
	if constexpr (index < std::tuple_size<decltype(fields)>::value)
	{
		auto pmem = std::get<index>(fields);
		serialize(value.*pmem, ctx);
		serialize_struct<index+1>(value, ctx);
	}
}

template <typename T>
std::enable_if_t<is_struct_with_fields<T>::value>
serialize(
	[[maybe_unused]] const T& input,
	[[maybe_unused]] serialization_context& ctx
) noexcept
{
	// For commands, add the command ID. Commands are only sent by the client,
	// so this is not considered in the deserialization functions.
	if constexpr (is_command<T>::value)
	{
		serialize(int1(T::command_id), ctx);
	}
	serialize_struct<0>(input, ctx);
}

template <std::size_t index, typename T>
std::size_t get_size_struct(
	[[maybe_unused]] const T& input,
	[[maybe_unused]] const serialization_context& ctx
) noexcept
{
	constexpr auto fields = get_struct_fields<T>::value;
	if constexpr (index == std::tuple_size<decltype(fields)>::value)
	{
		return 0;
	}
	else
	{
		constexpr auto pmem = std::get<index>(fields);
		return get_size_struct<index+1>(input, ctx) +
		       get_size(input.*pmem, ctx);
	}
}

template <typename T>
std::enable_if_t<is_struct_with_fields<T>::value, std::size_t>
get_size(const T& input, const serialization_context& ctx) noexcept
{
	std::size_t res = is_command<T>::value ? 1 : 0;
	res += get_size_struct<0>(input, ctx);
	return res;
}

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
inline std::size_t get_size(dummy_serializable, const serialization_context&) noexcept { return 0; }
inline void serialize(dummy_serializable, serialization_context&) noexcept {}
inline errc deserialize(dummy_serializable, deserialization_context&) noexcept { return errc::ok; }


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

} // detail
} // mysql
} // boost

#include "boost/mysql/detail/protocol/impl/serialization.ipp"

#endif
