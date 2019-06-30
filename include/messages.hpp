#ifndef MESSAGES_H_
#define MESSAGES_H_

#include <string>
#include "basic_types.hpp"

namespace mysql
{

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


enum server_status_flags
{
	SERVER_STATUS_IN_TRANS = 1 << 0,
	SERVER_STATUS_AUTOCOMMIT = 1 << 1,
	SERVER_MORE_RESULTS_EXISTS = 1 << 3,
	SERVER_QUERY_NO_GOOD_INDEX_USED = 1 << 4,
	SERVER_QUERY_NO_INDEX_USED = 1 << 5,
	SERVER_STATUS_CURSOR_EXISTS = 1 << 6,
	SERVER_STATUS_LAST_ROW_SENT = 1 << 7,
	SERVER_STATUS_DB_DROPPED = 1 << 8,
	SERVER_STATUS_NO_BACKSLASH_ESCAPES = 1 << 9,
	SERVER_STATUS_METADATA_CHANGED = 1 << 10,
	SERVER_QUERY_WAS_SLOW = 1 << 11,
	SERVER_PS_OUT_PARAMS = 1 << 12,
	SERVER_STATUS_IN_TRANS_READONLY = 1 << 13,
	SERVER_SESSION_STATE_CHANGED = 1 << 14
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
	int1 character_set; // default server a_protocol_character_set, only the lower 8-bits
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
	int1 character_set;
	// string[23] 	filler 	filler to the size of the handhshake response packet. All 0s.
	string_null username;
	string_lenenc auth_response; // we should set CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA
	string_null database; // we should set CLIENT_CONNECT_WITH_DB
	string_null client_plugin_name; // we should set CLIENT_PLUGIN_AUTH
	// TODO: CLIENT_CONNECT_ATTRS
};

}





#endif /* MESSAGES_H_ */
