/*
 * deserialization.cpp
 *
 *  Created on: Jun 30, 2019
 *      Author: ruben
 */

#include "deserialization.hpp"
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
		throw std::runtime_error {"Overflow (null-terminated string)"};
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

// Packet serialization and deserialization
mysql::ReadIterator mysql::deserialize(ReadIterator from, ReadIterator last, PacketHeader& output)
{
	from = deserialize(from, last, output.packet_size);
	from = deserialize(from, last, output.sequence_number);
	return from;
}

mysql::ReadIterator mysql::deserialize(ReadIterator from, ReadIterator last, OkPacket& output)
{
	// TODO: is packet header to be deserialized as part of this?
	from = deserialize(from, last, output.affected_rows);
	from = deserialize(from, last, output.last_insert_id);
	from = deserialize(from, last, output.status_flags);
	from = deserialize(from, last, output.warnings);
	from = deserialize(from, last, output.info);
	return from;
}

mysql::ReadIterator mysql::deserialize(ReadIterator from, ReadIterator last, ErrPacket& output)
{
	// TODO: is packet header to be deserialized as part of this?
	from = deserialize(from, last, output.error_code);
	from = deserialize(from, last, output.sql_state_marker);
	from = deserialize(from, last, output.sql_state);
	from = deserialize(from, last, output.error_message);
	return from;
}

mysql::ReadIterator mysql::deserialize(ReadIterator from, ReadIterator last, Handshake& output)
{
	// TODO: is protocol version (seems similar to packet header) to be deserialized as part of this?
	// TODO: we can improve efficiency here
	string_fixed<8> auth_plugin_data_part_1;
	int1 filler; // should be 0
	int1 auth_plugin_data_len;
	string_fixed<10> reserved;
	std::string_view auth_plugin_data_part_2;
	from = deserialize(from, last, output.server_version);
	from = deserialize(from, last, output.connection_id);
	from = deserialize(from, last, auth_plugin_data_part_1);
	from = deserialize(from, last, filler); // TODO: check if 0
	from = deserialize(from, last, &output.capability_falgs, 2); // lower 2 bytes
	from = deserialize(from, last, output.character_set);
	from = deserialize(from, last, output.status_flags);
	from = deserialize(from, last, reinterpret_cast<uint8_t*>(&output.capability_falgs)+2, 2); // higher 2 bytes
	from = deserialize(from, last, auth_plugin_data_len);
	from = deserialize(from, last, reserved);
	from = deserialize(from, last, auth_plugin_data_part_2, max(13, auth_plugin_data_len - 8));
	from = deserialize(from, last, output.auth_plugin_name);
	output.auth_plugin_data = auth_plugin_data_part_1;
	output.auth_plugin_data += auth_plugin_data_part_2;
	output.auth_plugin_data.pop_back(); // includes a null byte at the end
	boost::endian::little_to_native_inplace(output.capability_falgs);
	return from;
}

void mysql::serialize(DynamicBuffer& buffer, const HandshakeResponse& value)
{
	serialize(buffer, value.client_flag);
	serialize(buffer, value.max_packet_size);
	serialize(buffer, value.character_set);
	serialize(buffer, string_fixed<23>{}); // filler
	serialize(buffer, value.username);
	serialize(buffer, value.auth_response);
	serialize(buffer, value.database);
	serialize(buffer, value.client_plugin_name);
}


