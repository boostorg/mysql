/*
 * message_serialization.cpp
 *
 *  Created on: Jul 7, 2019
 *      Author: ruben
 */

#include "message_serialization.hpp"
#include "null_bitmap.hpp"
#include <bitset>
#include <ostream>
#include <memory>

using namespace std;

// General
mysql::ReadIterator mysql::deserialize(ReadIterator from, ReadIterator last, PacketHeader& output)
{
	from = deserialize(from, last, output.packet_size);
	from = deserialize(from, last, output.sequence_number);
	return from;
}

void mysql::serialize(DynamicBuffer& buffer, const PacketHeader& value)
{
	serialize(buffer, value.packet_size);
	serialize(buffer, value.sequence_number);
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

// Connection phase
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

// Resultsets
mysql::ReadIterator mysql::deserialize(ReadIterator from, ReadIterator last, ColumnDefinition& output)
{
	int_lenenc length_fixed_length_fields;
	from = deserialize(from, last, output.catalog);
	from = deserialize(from, last, output.schema);
	from = deserialize(from, last, output.table);
	from = deserialize(from, last, output.org_table);
	from = deserialize(from, last, output.name);
	from = deserialize(from, last, output.org_name);
	from = deserialize(from, last, length_fixed_length_fields);
	from = deserialize(from, last, output.character_set);
	from = deserialize(from, last, output.column_length);
	from = deserialize(from, last, output.type);
	from = deserialize(from, last, output.flags);
	from = deserialize(from, last, output.decimals);
	return from;
}

void mysql::serialize_binary_value(DynamicBuffer& buffer, const BinaryValue& value)
{
	std::visit([&buffer](auto v) {
		mysql::serialize(buffer, v);
	}, value);
}

// Prepared statements
void mysql::serialize(DynamicBuffer& buffer, const StmtPrepare& value)
{
	serialize(buffer, Command::COM_STMT_PREPARE);
	serialize(buffer, value.statement);
}

mysql::ReadIterator mysql::deserialize(ReadIterator from, ReadIterator last, StmtPrepareResponseHeader& output)
{
	// TODO: int1 status: must be 0 to be deserialized as part of this?
	int1 reserved;
	from = deserialize(from, last, output.statement_id);
	from = deserialize(from, last, output.num_columns);
	from = deserialize(from, last, output.num_params);
	from = deserialize(from, last, reserved);
	// TODO: warning_count appears to be optional but we are always requiring it
	from = deserialize(from, last, output.warning_count);
	return from;
}

void mysql::serialize(DynamicBuffer& buffer, const StmtExecute& value)
{
	serialize(buffer, Command::COM_STMT_EXECUTE);
	serialize(buffer, value.statement_id);
	serialize(buffer, value.flags);
	serialize(buffer, int4(1)); // iteration_count

	if (value.num_params > 0)
	{
		// NULL bitmap
		StmtExecuteNullBitmapTraits traits { value.num_params };
		std::vector<std::uint8_t> null_bitmap (traits.byte_count(), 0);
		if (value.new_params_bind_flag)
		{
			for (std::size_t i = 0; i < value.param_values.size(); ++i)
			{
				if (std::holds_alternative<std::nullptr_t>(value.param_values[i]))
				{
					traits.set_null(null_bitmap.data(), i);
				}
			}
		}
		buffer.add(null_bitmap.data(), null_bitmap.size());

		serialize(buffer, value.new_params_bind_flag);

		if (value.new_params_bind_flag)
		{
			for (const auto& param: value.param_values)
			{
				auto type = compute_field_type(param);
				serialize(buffer, type.first);
				serialize(buffer, int1(type.second ? 0 : 0x80));
			}

			for (const auto& param: value.param_values)
			{
				serialize_binary_value(buffer, param);
			}
		}
	}
}

mysql::ReadIterator mysql::deserialize(ReadIterator from, ReadIterator last, StmtExecuteResponseHeader& output)
{
	// TODO: int1 status: must be 0 to be deserialized as part of this?
	return deserialize(from, last, output.num_fields);
}


// TODO: refactor this
#define MYSQL_COMPUTE_FIELD_TYPE_IMPL_ENTRY(type, typenum, issigned) \
		template <> \
		constexpr std::pair<mysql::FieldType, bool> \
		compute_field_type_impl<type>() { return { mysql::FieldType::typenum, issigned }; };

template <typename T> constexpr std::pair<mysql::FieldType, bool> compute_field_type_impl();
MYSQL_COMPUTE_FIELD_TYPE_IMPL_ENTRY(std::int8_t, TINY, true);
MYSQL_COMPUTE_FIELD_TYPE_IMPL_ENTRY(std::uint8_t, TINY, false);
MYSQL_COMPUTE_FIELD_TYPE_IMPL_ENTRY(std::int16_t, SHORT, true);
MYSQL_COMPUTE_FIELD_TYPE_IMPL_ENTRY(std::uint16_t, SHORT, false);
MYSQL_COMPUTE_FIELD_TYPE_IMPL_ENTRY(std::int32_t, LONG, true);
MYSQL_COMPUTE_FIELD_TYPE_IMPL_ENTRY(std::uint32_t, LONG, false);
MYSQL_COMPUTE_FIELD_TYPE_IMPL_ENTRY(std::int64_t, LONGLONG, true);
MYSQL_COMPUTE_FIELD_TYPE_IMPL_ENTRY(std::uint64_t, LONGLONG, false);
MYSQL_COMPUTE_FIELD_TYPE_IMPL_ENTRY(mysql::string_lenenc, STRING, true);
MYSQL_COMPUTE_FIELD_TYPE_IMPL_ENTRY(std::nullptr_t, NULL_, true);

std::pair<mysql::FieldType, bool> mysql::compute_field_type(const BinaryValue& v)
{
	return std::visit([](auto elm) {
		return compute_field_type_impl<decltype(elm)>();
	}, v);
}

// Text serialization
std::ostream& mysql::operator<<(std::ostream& os, const Handshake& value)
{
	return os << "mysql::Handshake(\n"
			"  server_version=" << value.server_version.value << ",\n"
			"  connection_id="  << value.connection_id  << ",\n"
			"  auth_plugin_data=" << value.auth_plugin_data << ",\n"
			"  capability_falgs=" << std::bitset<32>{value.capability_falgs} << ",\n"
			"  character_set=" << static_cast<int1>(value.character_set) << ",\n"
			"  status_flags=" << std::bitset<16>{value.status_flags} << ",\n"
			"  auth_plugin_name=" << value.auth_plugin_name.value << "\n)";
}

std::ostream& mysql::operator<<(std::ostream& os, const HandshakeResponse& value)
{
	return os << "mysql::HandshakeResponse(\n"
			"  client_flag(capabilities)=" << std::bitset<32>(value.client_flag) << ",\n"
			"  max_packet_size="  << value.max_packet_size  << ",\n"
			"  character_set="  << static_cast<int1>(value.character_set) << ",\n"
			"  username=" << value.username.value << ",\n"
			"  auth_response=" << value.auth_response.value << ",\n"
			"  database=" << value.database.value << ",\n"
			"  client_plugin_name=" << value.client_plugin_name.value << "\n)";
}
