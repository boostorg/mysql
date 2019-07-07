/*
 * deserialization.cpp
 *
 *  Created on: Jun 30, 2019
 *      Author: ruben
 */

#include <basic_serialization.hpp>
#include <stdexcept>
#include <algorithm>

using namespace std;

void mysql::check_size(ReadIterator from, ReadIterator last, std::size_t sz)
{
	if ((last - from) < sz)
		throw std::out_of_range {"Overflow"};
}

mysql::ReadIterator mysql::deserialize(ReadIterator from, ReadIterator last, int_lenenc& output)
{
	std::uint8_t first_byte;
	from = deserialize(from, last, first_byte);
	if (first_byte == 0xFC)
	{
		int2 value;
		from = deserialize(from, last, value);
		output.value = value;
	}
	else if (first_byte == 0xFD)
	{
		int3 value;
		from = deserialize(from, last, value);
		output.value = value.value;
	}
	else if (first_byte == 0xFE)
	{
		from = deserialize(from, last, output.value);
	}
	else
	{
		output.value = first_byte;
	}
	return from;
}

mysql::ReadIterator mysql::deserialize(ReadIterator from, ReadIterator last, string_null& output)
{
	ReadIterator string_end = std::find(from, last, 0);
	if (string_end == last)
		throw std::out_of_range {"Overflow (null-terminated string)"};
	output.value = get_string(from, string_end-from);
	return string_end + 1; // skip the null terminator
}

void mysql::serialize(DynamicBuffer& buffer, int_lenenc value)
{
	if (value.value < 251)
	{
		serialize(buffer, static_cast<int1>(value.value));
	}
	else if (value.value < 0x10000)
	{
		serialize(buffer, int1(0xfc));
		serialize(buffer, static_cast<int2>(value.value));
	}
	else if (value.value < 0x1000000)
	{
		serialize(buffer, int1(0xfd));
		serialize(buffer, int3 {static_cast<uint32_t>(value.value)});
	}
	else
	{
		serialize(buffer, int1(0xfe));
		serialize(buffer, static_cast<int8>(value.value));
	}
}



