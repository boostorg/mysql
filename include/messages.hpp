#ifndef MESSAGES_H_
#define MESSAGES_H_

#include <string>
#include <vector>
#include <variant>
#include "basic_types.hpp"

namespace mysql
{

// Server/client capabilities
constexpr int4 CLIENT_LONG_PASSWORD = 1; // Use the improved version of Old Password Authentication
constexpr int4 CLIENT_FOUND_ROWS = 2; // Send found rows instead of affected rows in EOF_Packet
constexpr int4 CLIENT_LONG_FLAG = 4; // Get all column flags
constexpr int4 CLIENT_CONNECT_WITH_DB = 8; // Database (schema) name can be specified on connect in Handshake Response Packet
constexpr int4 CLIENT_NO_SCHEMA = 16; // Don't allow database.table.column
constexpr int4 CLIENT_COMPRESS = 32; // Compression protocol supported
constexpr int4 CLIENT_ODBC = 64; // Special handling of ODBC behavior
constexpr int4 CLIENT_LOCAL_FILES = 128; // Can use LOAD DATA LOCAL
constexpr int4 CLIENT_IGNORE_SPACE = 256; // Ignore spaces before '('
constexpr int4 CLIENT_PROTOCOL_41 = 512; // New 4.1 protocol
constexpr int4 CLIENT_INTERACTIVE = 1024; // This is an interactive client
constexpr int4 CLIENT_SSL = 2048; // Use SSL encryption for the session
constexpr int4 CLIENT_IGNORE_SIGPIPE = 4096; // Client only flag
constexpr int4 CLIENT_TRANSACTIONS = 8192; // Client knows about transactions
constexpr int4 CLIENT_RESERVED = 16384; // DEPRECATED: Old flag for 4.1 protocol
constexpr int4 CLIENT_RESERVED2 = 32768; // DEPRECATED: Old flag for 4.1 authentication \ CLIENT_SECURE_CONNECTION
constexpr int4 CLIENT_MULTI_STATEMENTS = (1UL << 16); // Enable/disable multi-stmt support
constexpr int4 CLIENT_MULTI_RESULTS = (1UL << 17); // Enable/disable multi-results
constexpr int4 CLIENT_PS_MULTI_RESULTS = (1UL << 18); // Multi-results and OUT parameters in PS-protocol
constexpr int4 CLIENT_PLUGIN_AUTH = (1UL << 19); // Client supports plugin authentication
constexpr int4 CLIENT_CONNECT_ATTRS = (1UL << 20); // Client supports connection attributes
constexpr int4 CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA = (1UL << 21); // Enable authentication response packet to be larger than 255 bytes
constexpr int4 CLIENT_CAN_HANDLE_EXPIRED_PASSWORDS = (1UL << 22); // Don't close the connection for a user account with expired password
constexpr int4 CLIENT_SESSION_TRACK = (1UL << 23); // Capable of handling server state change information
constexpr int4 CLIENT_DEPRECATE_EOF = (1UL << 24); // Client no longer needs EOF_Packet and will use OK_Packet instead
constexpr int4 CLIENT_SSL_VERIFY_SERVER_CERT = (1UL << 30); // Verify server certificate
constexpr int4 CLIENT_OPTIONAL_RESULTSET_METADATA = (1UL << 25); // The client can handle optional metadata information in the resultset
constexpr int4 CLIENT_REMEMBER_OPTIONS = (1UL << 31); // Don't reset the options after an unsuccessful connect

// Server status flags
constexpr int4 SERVER_STATUS_IN_TRANS = 1;
constexpr int4 SERVER_STATUS_AUTOCOMMIT = 2;
constexpr int4 SERVER_MORE_RESULTS_EXISTS = 8;
constexpr int4 SERVER_QUERY_NO_GOOD_INDEX_USED = 16;
constexpr int4 SERVER_QUERY_NO_INDEX_USED = 32;
constexpr int4 SERVER_STATUS_CURSOR_EXISTS = 64;
constexpr int4 SERVER_STATUS_LAST_ROW_SENT = 128;
constexpr int4 SERVER_STATUS_DB_DROPPED = 256;
constexpr int4 SERVER_STATUS_NO_BACKSLASH_ESCAPES = 512;
constexpr int4 SERVER_STATUS_METADATA_CHANGED = 1024;
constexpr int4 SERVER_QUERY_WAS_SLOW = 2048;
constexpr int4 SERVER_PS_OUT_PARAMS = 4096;
constexpr int4 SERVER_STATUS_IN_TRANS_READONLY = 8192;
constexpr int4 SERVER_SESSION_STATE_CHANGED = (1UL << 14) ;

enum class CharacterSetLowerByte : int1
{
	latin1_swedish_ci = 0x08,
	utf8_general_ci = 0x21,
	binary = 0x3f
};

// Packet type constants
constexpr int1 handshake_protocol_version_9 = 9;
constexpr int1 handshake_protocol_version_10 = 10;
constexpr int1 error_packet_header = 0xff;
constexpr int1 ok_packet_header = 0x00;
constexpr int1 eof_packet_header = 0xfe;

struct PacketHeader
{
	int3 packet_size;
	int1 sequence_number;
};

struct OkPacket
{
	// header: int<1> 	header 	0x00 or 0xFE the OK packet header
	int_lenenc affected_rows;
	int_lenenc last_insert_id;
	int2 status_flags; // server_status_flags
	int2 warnings;
	// TODO: CLIENT_SESSION_TRACK
	string_eof info;
};

struct ErrPacket
{
	// int<1> 	header 	0xFF ERR packet header
	int2 error_code;
	string_fixed<1> sql_state_marker;
	string_fixed<5> sql_state;
	string_eof error_message;
};

struct Handshake
{
	// int<1> 	protocol version 	Always 10
	string_null server_version;
	int4 connection_id;
	std::string auth_plugin_data; // merge of the two parts - not an actual field
	int4 capability_falgs; // merge of the two parts - not an actual field
	// string[8] 	auth-plugin-data-part-1 	first 8 bytes of the plugin provided data (scramble)
	// int<1> 	filler 	0x00 byte, terminating the first part of a scramble
	// int<2> 	capability_flags_1 	The lower 2 bytes of the Capabilities Flags
	CharacterSetLowerByte character_set; // default server a_protocol_character_set, only the lower 8-bits
	int2 status_flags; // server_status_flags
	// int<2> 	capability_flags_2 	The upper 2 bytes of the Capabilities Flags
	// int<1> 	auth_plugin_data_len
	// string[10] 	reserved 	reserved. All 0s.
	// $length 	auth-plugin-data-part-2
	string_null auth_plugin_name;
};

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

enum class Command : int1
{
	COM_QUIT = 1,
	COM_INIT_DB = 2,
	COM_QUERY = 3,
	COM_STATISTICS = 8,
	COM_DEBUG = 0x0d,
	COM_PING = 0x0e,
	COM_CHANGE_USER = 0x11,
	COM_BINLOG_DUMP = 0x12,
	COM_STMT_PREPARE = 0x16,
	COM_STMT_EXECUTE = 0x17,
	COM_STMT_SEND_LONG_DATA = 0x18,
	COM_STMT_CLOSE = 0x19,
	COM_STMT_RESET = 0x1a,
	COM_SET_OPTION = 0x1b,
	COM_STMT_FETCH = 0x1c,
	COM_RESET_CONNECTION = 0x1f
};

// Column definitions
enum class FieldType : int1
{
	DECIMAL = 0x00,
	TINY = 0x01,
	SHORT = 0x02,
	LONG = 0x03,
	FLOAT = 0x04,
	DOUBLE = 0x05,
	NULL_ = 0x06,
	TIMESTAMP = 0x07,
	LONGLONG = 0x08,
	INT24 = 0x09,
	DATE = 0x0a,
	TIME = 0x0b,
	DATETIME = 0x0c,
	YEAR = 0x0d,
	VARCHAR = 0x0f,
	BIT = 0x10,
	NEWDECIMAL = 0xf6,
	ENUM = 0xf7,
	SET = 0xf8,
	TINY_BLOB = 0xf9,
	MEDIUM_BLOB = 0xfa,
	LONG_BLOB = 0xfb,
	BLOB = 0xfc,
	VAR_STRING = 0xfd,
	STRING = 0xfe,
	GEOMETRY = 0xff
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

// Prepared statements
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
	string_lenenc,
	int8,
	int4,
	int2,
	int1,
	// TODO: double, float, dates/times
	nullptr_t // aka NULL
>;

struct StmtParamValue
{
	FieldType field_type;
	bool is_signed;
	BinaryValue value;
};

struct StmtExecute
{
	//int1 message_type: COM_STMT_EXECUTE
	int4 statement_id;
	int1 flags;
	// int4 iteration_count: always 1
	int1 new_params_bind_flag;
	std::vector<StmtParamValue> param_values;
};


}





#endif /* MESSAGES_H_ */
