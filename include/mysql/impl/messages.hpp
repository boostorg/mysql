#ifndef MESSAGES_H_
#define MESSAGES_H_

#include <string>
#include <vector>
#include <variant>
#include <tuple>
#include "mysql/impl/basic_types.hpp"
#include "mysql/impl/constants.hpp"
#include "mysql/impl/basic_serialization.hpp"

namespace mysql
{
namespace detail
{

namespace msgs
{

struct packet_header
{
	int3 packet_size;
	int1 sequence_number;

	static constexpr auto fields = std::make_tuple(
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
	// TODO: CLIENT_SESSION_TRACK. This may require separate serialization functions
	string_lenenc info;

	static constexpr auto fields = std::make_tuple(
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

	static constexpr auto fields = std::make_tuple(
		&err_packet::error_code,
		&err_packet::sql_state_marker,
		&err_packet::sql_state,
		&err_packet::error_message
	);
};

struct handshake
{
	// int<1> 	protocol version 	Always 10
	string_null server_version;
	int4 connection_id;
	string_lenenc auth_plugin_data; // not an actual protocol field, the merge of two fields
	int4 capability_falgs; // merge of the two parts - not an actual field
	CharacterSetLowerByte character_set; // default server a_protocol_character_set, only the lower 8-bits
	int2 status_flags; // server_status_flags
	string_null auth_plugin_name;

	std::array<char, 8 + 0xff> auth_plugin_data_buffer; // not an actual protocol field, the merge of two fields

	static constexpr auto fields = std::make_tuple(
		&handshake::server_version,
		&handshake::connection_id,
		&handshake::auth_plugin_data,
		&handshake::capability_falgs,
		&handshake::character_set,
		&handshake::status_flags,
		&handshake::auth_plugin_name
	);
};

} // msgs

// serialization functions
inline Error deserialize(msgs::ok_packet& output, DeserializationContext& ctx) noexcept;
inline Error deserialize(msgs::handshake& output, DeserializationContext& ctx) noexcept;



struct HandshakeResponse
{
	int4 client_flag; // capabilities
	int4 max_packet_size;
	CharacterSetLowerByte character_set;
	// string[23] 	filler 	filler to the size of the handhshake response packet. All 0s.
	string_null username;
	string_lenenc auth_response; // we should set CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA
	string_null database; // we should set CLIENT_CONNECT_WITH_DB
	string_null client_plugin_name; // we should set CLIENT_PLUGIN_AUTH
	// TODO: CLIENT_CONNECT_ATTRS
};



struct ColumnDefinition
{
	string_lenenc catalog; // always "def"
	string_lenenc schema;
	string_lenenc table; // virtual table
	string_lenenc org_table; // physical table
	string_lenenc name; // virtual column name
	string_lenenc org_name; // physical column name
	// int<lenenc> 	length of fixed length fields 	[0x0c]
	int2 character_set; // TODO: enum-erize this
	int4 column_length; // maximum length of the field
	FieldType type; // type of the column as defined in enum_field_types
	int2 flags; // Flags as defined in Column Definition Flags
	int1 decimals; // max shown decimal digits. 0x00 for int/static strings; 0x1f for dynamic strings, double, float
};



struct StmtPrepare
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
};


} // detail
} // mysql

#include "mysql/impl/messages_impl.hpp"


#endif /* MESSAGES_H_ */
