#ifndef DESERIALIZATION_H_
#define DESERIALIZATION_H_

#include <boost/endian/conversion.hpp>
#include "basic_types.hpp"

namespace mysql
{

// Utility
void check_size(ReadIterator from, ReadIterator last, std::size_t sz);

inline std::string_view get_string(ReadIterator from, std::size_t size)
{
	return std::string_view {reinterpret_cast<const char*>(from), size};
}

// Fixed size
template <typename T> struct get_size { static constexpr std::size_t value = std::size_t(-1); };
template <> struct get_size<int1> { static constexpr std::size_t value = 1; };
template <> struct get_size<int2> { static constexpr std::size_t value = 2; };
template <> struct get_size<int3> { static constexpr std::size_t value = 3; };
template <> struct get_size<int4> { static constexpr std::size_t value = 4; };
template <> struct get_size<int6> { static constexpr std::size_t value = 6; };
template <> struct get_size<int8> { static constexpr std::size_t value = 8; };
template <std::size_t size> struct get_size<string_fixed<size>> { static constexpr std::size_t value = size; };

template <typename T> constexpr std::size_t get_size_v = get_size<T>::value;
template <typename T> constexpr bool is_fixed_size_v = get_size_v<T> != std::size_t(-1);

template <typename T> void little_to_native(T& value) { boost::endian::little_to_native_inplace(value); }
template <std::size_t size> void little_to_native(string_fixed<size>&) {}

// Deserialization functions
template <typename T>
std::enable_if_t<is_fixed_size_v<T>, ReadIterator>
deserialize(ReadIterator from, ReadIterator last, T& output)
{
	check_size(from, last, get_size_v<T>);
	memcpy(&output, from, get_size_v<T>);
	little_to_native(output);
	return from + get_size_v<T>;
}

ReadIterator deserialize(ReadIterator from, ReadIterator last, int_lenenc& output);
ReadIterator deserialize(ReadIterator from, ReadIterator last, string_null& output);

inline ReadIterator deserialize(ReadIterator from, ReadIterator last, string_eof& output)
{
	output.value = get_string(from, last-from);
	return last;
}


}



#endif /* DESERIALIZATION_H_ */
