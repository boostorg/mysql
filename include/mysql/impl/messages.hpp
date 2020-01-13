#ifndef MYSQL_ASIO_IMPL_MESSAGES_HPP
#define MYSQL_ASIO_IMPL_MESSAGES_HPP

#include "mysql/impl/serialization.hpp"
#include <string>
#include <vector>
#include <variant>
#include <tuple>
#include "mysql/impl/basic_types.hpp"
#include "mysql/impl/constants.hpp"
#include "mysql/collation.hpp"

namespace mysql
{
namespace detail
{

struct packet_header
{
	int3 packet_size;
	int1 sequence_number;
};

template <>
struct get_struct_fields<packet_header>
{
	static constexpr auto value = std::make_tuple(
		&packet_header::packet_size,
		&packet_header::sequence_number
	);
};

struct ok_packet
{
	// header: int<1> 	header 	0x00 or 0xFE the OK packet header
	int_lenenc affected_rows;
	int_lenenc last_insert_id;
	int2 status_flags; // server_status_flags
	int2 warnings;
	// TODO: CLIENT_SESSION_TRACK
	string_lenenc info;
};

template <>
struct get_struct_fields<ok_packet>
{
	static constexpr auto value = std::make_tuple(
		&ok_packet::affected_rows,
		&ok_packet::last_insert_id,
		&ok_packet::status_flags,
		&ok_packet::warnings,
		&ok_packet::info
	);
};

struct err_packet
{
	// int<1> 	header 	0xFF ERR packet header
	int2 error_code;
	string_fixed<1> sql_state_marker;
	string_fixed<5> sql_state;
	string_eof error_message;
};

template <>
struct get_struct_fields<err_packet>
{
	static constexpr auto value = std::make_tuple(
		&err_packet::error_code,
		&err_packet::sql_state_marker,
		&err_packet::sql_state,
		&err_packet::error_message
	);
};

struct handshake_packet
{
	// int<1> 	protocol version 	Always 10
	string_null server_version;
	int4 connection_id;
	string_lenenc auth_plugin_data; // not an actual protocol field, the merge of two fields
	int4 capability_falgs; // merge of the two parts - not an actual field
	int1 character_set; // default server a_protocol_character_set, only the lower 8-bits
	int2 status_flags; // server_status_flags
	string_null auth_plugin_name;

	std::array<char, 8 + 0xff> auth_plugin_data_buffer; // not an actual protocol field, the merge of two fields
};

template <>
struct get_struct_fields<handshake_packet>
{
	static constexpr auto value = std::make_tuple(
		&handshake_packet::server_version,
		&handshake_packet::connection_id,
		&handshake_packet::auth_plugin_data,
		&handshake_packet::capability_falgs,
		&handshake_packet::character_set,
		&handshake_packet::status_flags,
		&handshake_packet::auth_plugin_name
	);
};

struct handshake_response_packet
{
	int4 client_flag; // capabilities
	int4 max_packet_size;
	int1 character_set;
	// string[23] 	filler 	filler to the size of the handhshake response packet. All 0s.
	string_null username;
	string_lenenc auth_response; // we require CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA
	string_null database; // only to be serialized if CLIENT_CONNECT_WITH_DB
	string_null client_plugin_name; // we require CLIENT_PLUGIN_AUTH
	// TODO: CLIENT_CONNECT_ATTRS
};

template <>
struct get_struct_fields<handshake_response_packet>
{
	static constexpr auto value = std::make_tuple(
		&handshake_response_packet::client_flag,
		&handshake_response_packet::max_packet_size,
		&handshake_response_packet::character_set,
		&handshake_response_packet::username,
		&handshake_response_packet::auth_response,
		&handshake_response_packet::database,
		&handshake_response_packet::client_plugin_name
	);
};

struct auth_switch_request_packet
{
	string_null plugin_name;
	string_eof auth_plugin_data;
};

template <>
struct get_struct_fields<auth_switch_request_packet>
{
	static constexpr auto value = std::make_tuple(
		&auth_switch_request_packet::plugin_name,
		&auth_switch_request_packet::auth_plugin_data
	);
};

struct auth_switch_response_packet
{
	string_eof auth_plugin_data;
};

template <>
struct get_struct_fields<auth_switch_response_packet>
{
	static constexpr auto value = std::make_tuple(
		&auth_switch_response_packet::auth_plugin_data
	);
};

struct column_definition_packet
{
	string_lenenc catalog; // always "def"
	string_lenenc schema;
	string_lenenc table; // virtual table
	string_lenenc org_table; // physical table
	string_lenenc name; // virtual column name
	string_lenenc org_name; // physical column name
	collation character_set;
	int4 column_length; // maximum length of the field
	protocol_field_type type; // type of the column as defined in enum_field_types
	int2 flags; // Flags as defined in Column Definition Flags
	int1 decimals; // max shown decimal digits. 0x00 for int/static strings; 0x1f for dynamic strings, double, float
};

template <>
struct get_struct_fields<column_definition_packet>
{
	static constexpr auto value = std::make_tuple(
		&column_definition_packet::catalog,
		&column_definition_packet::schema,
		&column_definition_packet::table,
		&column_definition_packet::org_table,
		&column_definition_packet::name,
		&column_definition_packet::org_name,
		&column_definition_packet::character_set,
		&column_definition_packet::column_length,
		&column_definition_packet::type,
		&column_definition_packet::flags,
		&column_definition_packet::decimals
	);
};

// Commands
struct com_query_packet
{
	string_eof query;

	static constexpr std::uint8_t command_id = 3;
};

template <>
struct get_struct_fields<com_query_packet>
{
	static constexpr auto value = std::make_tuple(
		&com_query_packet::query
	);
};


// serialization functions
inline Error deserialize(ok_packet& output, DeserializationContext& ctx) noexcept;
inline Error deserialize(handshake_packet& output, DeserializationContext& ctx) noexcept;
inline std::size_t get_size(const handshake_response_packet& value, const SerializationContext& ctx) noexcept;
inline void serialize(const handshake_response_packet& value, SerializationContext& ctx) noexcept;
inline Error deserialize(auth_switch_request_packet& output, DeserializationContext& ctx) noexcept;
inline Error deserialize(column_definition_packet& output, DeserializationContext& ctx) noexcept;

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
	DeserializationContext& ctx
);

inline std::pair<error_code, std::uint8_t> deserialize_message_type(
	DeserializationContext& ctx
);

inline error_code process_error_packet(DeserializationContext& ctx);


/*struct StmtPrepare
{
	string_eof statement;
};

struct StmtPrepareResponseHeader
{
	// int1 status: must be 0
	int4 statement_id;
	int2 num_columns;
	int2 num_params;
	// int1 reserved_1: must be 0
	int2 warning_count; // only if (packet_length > 12)
	// TODO: int1 metadata_follows when CLIENT_OPTIONAL_RESULTSET_METADATA
};

using BinaryValue = std::variant<
	std::int8_t,
	std::int16_t,
	std::int32_t,
	std::int64_t,
	std::uint8_t,
	std::uint16_t,
	std::uint32_t,
	std::uint64_t,
	string_lenenc,
	std::nullptr_t // NULL
	// TODO: double, float, dates/times
>;

struct StmtExecute
{
	//int1 message_type: COM_STMT_EXECUTE
	int4 statement_id;
	int1 flags;
	// int4 iteration_count: always 1
	int1 num_params;
	int1 new_params_bind_flag;
	std::vector<BinaryValue> param_values; // empty if !new_params_bind_flag
};

struct StmtExecuteResponseHeader
{
	int1 num_fields;
};

struct StmtFetch
{
	// int1 message_type: COM_STMT_FETCH
	int4 statement_id;
	int4 rows_to_fetch;
};

struct StmtClose
{
	int4 statement_id;
};*/


} // detail
} // mysql

#include "mysql/impl/messages.ipp"


#endif
