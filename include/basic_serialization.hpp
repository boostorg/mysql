#ifndef DESERIALIZATION_H_
#define DESERIALIZATION_H_

#include <boost/endian/conversion.hpp>
#include <vector>
#include <type_traits>
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
template <> inline void little_to_native(int3& value) { boost::endian::little_to_native_inplace(value.value); }
template <> inline void little_to_native(int6& value) { boost::endian::little_to_native_inplace(value.value); }
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

inline ReadIterator deserialize(ReadIterator from, ReadIterator last, std::string_view& output, std::size_t size)
{
	check_size(from, last, size);
	output = get_string(from, size);
	return from + size;
}

inline ReadIterator deserialize(ReadIterator from, ReadIterator last, void* output, std::size_t size)
{
	check_size(from, last, size);
	memcpy(output, from, size);
	return from + size;
}

inline ReadIterator deserialize(ReadIterator from, ReadIterator last, string_lenenc& output)
{
	int_lenenc length;
	from = deserialize(from, last, length);
	from = deserialize(from, last, output.value, length.value);
	return from;
}

template <typename T>
std::enable_if_t<std::is_enum_v<T>, ReadIterator>
deserialize(ReadIterator from, ReadIterator last, T& to)
{
	std::underlying_type_t<T> value;
	ReadIterator res = deserialize(from, last, value);
	to = static_cast<T>(value);
	return res;
}

// SERIALIZATION

class DynamicBuffer
{
	std::vector<std::uint8_t> buffer_;
public:
	DynamicBuffer(int1 sequence_number): buffer_ { 0, 0, 0, sequence_number } {};
	void add(const void* data, std::size_t size)
	{
		auto current_size = buffer_.size();
		buffer_.resize(current_size+size);
		memcpy(buffer_.data() + current_size, data, size);
	}
	void add(std::uint8_t value) { buffer_.push_back(value); }
	void set_size()
	{
		int3 packet_length { static_cast<std::uint32_t>(buffer_.size() - 4) };
		boost::endian::native_to_little_inplace(packet_length.value);
		memcpy(buffer_.data(), &packet_length.value, 3);
	}
	const void* data() const { return buffer_.data(); }
	std::size_t size() const { return buffer_.size(); }
};

template <typename T> void native_to_little(T& value) { boost::endian::native_to_little_inplace(value); }
template <> inline void native_to_little(int3& value) { boost::endian::native_to_little_inplace(value.value); }
template <> inline void native_to_little(int6& value) { boost::endian::native_to_little_inplace(value.value); }



template <typename T>
std::enable_if_t<is_fixed_size_v<T>>
serialize(DynamicBuffer& buffer, T value)
{
	native_to_little(value);
	buffer.add(&value, get_size_v<T>);
}

template <>
inline void serialize<int1>(DynamicBuffer& buffer, int1 value) { buffer.add(value); }

template <std::size_t size>
void serialize(DynamicBuffer& buffer, const string_fixed<size>& value)
{
	buffer.add(value, sizeof(value));
}

void serialize(DynamicBuffer& buffer, int_lenenc value);

inline void serialize(DynamicBuffer& buffer, const std::string_view& value)
{
	buffer.add(value.data(), value.size());
}

inline void serialize(DynamicBuffer& buffer, const string_null& value)
{
	serialize(buffer, value.value);
	serialize(buffer, int1(0));
}

inline void serialize(DynamicBuffer& buffer, const string_eof& value)
{
	serialize(buffer, value.value);
}

inline void serialize(DynamicBuffer& buffer, const string_lenenc& value)
{
	serialize(buffer, int_lenenc {value.value.size()});
	serialize(buffer, value.value);
}





}



#endif /* DESERIALIZATION_H_ */
