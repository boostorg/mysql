#ifndef MYSQL_ASIO_IMPL_BASIC_TYPES_HPP
#define MYSQL_ASIO_IMPL_BASIC_TYPES_HPP

#include "boost/mysql/detail/aux/value_holder.hpp"
#include <cstdint>
#include <string_view>
#include <array>
#include <vector>
#include <cstring>

namespace boost {
namespace mysql {
namespace detail {

struct int1 : value_holder<std::uint8_t> { using value_holder::value_holder; };
struct int2 : value_holder<std::uint16_t> { using value_holder::value_holder; };
struct int3 : value_holder<std::uint32_t> { using value_holder::value_holder; };
struct int4 : value_holder<std::uint32_t> { using value_holder::value_holder; };
struct int6 : value_holder<std::uint64_t> { using value_holder::value_holder; };
struct int8 : value_holder<std::uint64_t> { using value_holder::value_holder; };
struct int1_signed : value_holder<std::int8_t> { using value_holder::value_holder; };
struct int2_signed : value_holder<std::int16_t> { using value_holder::value_holder; };
struct int4_signed : value_holder<std::int32_t> { using value_holder::value_holder; };
struct int8_signed : value_holder<std::int64_t> { using value_holder::value_holder; };
struct int_lenenc : value_holder<std::uint64_t> { using value_holder::value_holder; };

template <std::size_t size>
struct string_fixed : value_holder<std::array<char, size>>
{
	constexpr string_fixed() = default;
	explicit string_fixed(const char (&v) [size])
	{
		std::memcpy(this->value.data(), v, size);
	}
};

struct string_null : value_holder<std::string_view> { using value_holder::value_holder; };
struct string_eof : value_holder<std::string_view> { using value_holder::value_holder; };
struct string_lenenc : value_holder<std::string_view> { using value_holder::value_holder; };


} // detail
} // mysql
} // boost

#endif
