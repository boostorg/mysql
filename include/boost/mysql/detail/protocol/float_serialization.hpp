#ifndef INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_FLOAT_SERIALIZATION_HPP_
#define INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_FLOAT_SERIALIZATION_HPP_

#include "boost/mysql/detail/protocol/serialization_context.hpp"
#include "boost/mysql/detail/protocol/deserialization_context.hpp"
#include <algorithm>
#include <type_traits>

namespace boost {
namespace mysql {
namespace detail {

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


}
}
}

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_FLOAT_SERIALIZATION_HPP_ */
