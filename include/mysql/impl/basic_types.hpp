#ifndef BASIC_TYPES_H_
#define BASIC_TYPES_H_

#include <cstdint>
#include <string_view>
#include <array>

namespace mysql
{
namespace detail
{

using ReadIterator = const std::uint8_t*;
using WriteIterator = std::uint8_t*;

template <typename T>
struct ValueHolder
{
	using value_type = T;

	value_type value;

	bool operator==(const ValueHolder<T>& rhs) const { return value == rhs.value; }
	bool operator!=(const ValueHolder<T>& rhs) const { return value != rhs.value; }
};

struct int1 : ValueHolder<std::uint8_t> {};
struct int2 : ValueHolder<std::uint16_t> {};
struct int3 : ValueHolder<std::uint32_t> {};
struct int4 : ValueHolder<std::uint32_t> {};
struct int6 : ValueHolder<std::uint64_t> {};
struct int8 : ValueHolder<std::uint64_t> {};
struct int1_signed : ValueHolder<std::int8_t> {};
struct int2_signed : ValueHolder<std::int16_t> {};
struct int4_signed : ValueHolder<std::int32_t> {};
struct int8_signed : ValueHolder<std::int64_t> {};
struct int_lenenc : ValueHolder<std::uint64_t> {};
template <std::size_t size> struct string_fixed : ValueHolder<std::array<char, size>> {};
struct string_null : ValueHolder<std::string_view> {};
struct string_eof : ValueHolder<std::string_view> {};
struct string_lenenc : ValueHolder<std::string_view> {};

}
}


#endif /* BASIC_TYPES_H_ */
