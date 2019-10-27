#ifndef INCLUDE_MYSQL_IMPL_CONSTANTS_HPP_
#define INCLUDE_MYSQL_IMPL_CONSTANTS_HPP_

#include "mysql/impl/basic_types.hpp"

namespace mysql
{
namespace detail
{

constexpr std::size_t MAX_PACKET_SIZE = 0xffffff;

// Server status flags
constexpr std::uint32_t SERVER_STATUS_IN_TRANS = 1;
constexpr std::uint32_t SERVER_STATUS_AUTOCOMMIT = 2;
constexpr std::uint32_t SERVER_MORE_RESULTS_EXISTS = 8;
constexpr std::uint32_t SERVER_QUERY_NO_GOOD_INDEX_USED = 16;
constexpr std::uint32_t SERVER_QUERY_NO_INDEX_USED = 32;
constexpr std::uint32_t SERVER_STATUS_CURSOR_EXISTS = 64;
constexpr std::uint32_t SERVER_STATUS_LAST_ROW_SENT = 128;
constexpr std::uint32_t SERVER_STATUS_DB_DROPPED = 256;
constexpr std::uint32_t SERVER_STATUS_NO_BACKSLASH_ESCAPES = 512;
constexpr std::uint32_t SERVER_STATUS_METADATA_CHANGED = 1024;
constexpr std::uint32_t SERVER_QUERY_WAS_SLOW = 2048;
constexpr std::uint32_t SERVER_PS_OUT_PARAMS = 4096;
constexpr std::uint32_t SERVER_STATUS_IN_TRANS_READONLY = 8192;
constexpr std::uint32_t SERVER_SESSION_STATE_CHANGED = (1UL << 14) ;

enum class CharacterSetLowerByte : std::uint8_t
{
	latin1_swedish_ci = 0x08,
	utf8_general_ci = 0x21,
	binary = 0x3f
};

// Packet type constants
constexpr std::uint8_t handshake_protocol_version_9 = 9;
constexpr std::uint8_t handshake_protocol_version_10 = 10;
constexpr std::uint8_t error_packet_header = 0xff;
constexpr std::uint8_t ok_packet_header = 0x00;
constexpr std::uint8_t eof_packet_header = 0xfe;
constexpr std::uint8_t auth_switch_request_header = 0xfe;

// Column flags
namespace column_flags
{
constexpr std::uint16_t not_null = 1;     // Field can't be NULL.
constexpr std::uint16_t pri_key = 2;     // Field is part of a primary key.
constexpr std::uint16_t unique_key = 4;     // Field is part of a unique key.
constexpr std::uint16_t multiple_key = 8;     // Field is part of a key.
constexpr std::uint16_t blob = 16;     // Field is a blob.
constexpr std::uint16_t unsigned_ = 32;     // Field is unsigned.
constexpr std::uint16_t zerofill = 64;     // Field is zerofill.
constexpr std::uint16_t binary = 128;     // Field is binary.
constexpr std::uint16_t enum_ = 256;     // field is an enum
constexpr std::uint16_t auto_increment = 512;     // field is a autoincrement field
constexpr std::uint16_t timestamp = 1024;     // Field is a timestamp.
constexpr std::uint16_t set = 2048;     // field is a set
constexpr std::uint16_t no_default_value = 4096;     // Field doesn't have default value.
constexpr std::uint16_t on_update_now = 8192;     // Field is set to NOW on UPDATE.
constexpr std::uint16_t part_key = 16384;     // Intern; Part of some key.
constexpr std::uint16_t num = 32768;     // Field is num (for clients)
}

enum class Command : std::uint8_t
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

// Prepared statements
constexpr std::uint8_t CURSOR_TYPE_NO_CURSOR = 0;
constexpr std::uint8_t CURSOR_TYPE_READ_ONLY = 1;
constexpr std::uint8_t CURSOR_TYPE_FOR_UPDATE = 2;
constexpr std::uint8_t CURSOR_TYPE_SCROLLABLE = 4;

}
}



#endif /* INCLUDE_MYSQL_IMPL_CONSTANTS_HPP_ */
