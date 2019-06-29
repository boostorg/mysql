#ifndef MESSAGES_H_
#define MESSAGES_H_

#include <string>
#include "basic_types.hpp"

namespace mysql
{

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



}





#endif /* MESSAGES_H_ */
