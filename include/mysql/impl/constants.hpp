#ifndef INCLUDE_MYSQL_IMPL_CONSTANTS_HPP_
#define INCLUDE_MYSQL_IMPL_CONSTANTS_HPP_

#include "mysql/impl/basic_types.hpp"

namespace mysql
{
namespace detail
{

constexpr std::size_t MAX_PACKET_SIZE = 0xffffff;

// Server/client capabilities
constexpr std::uint32_t CLIENT_LONG_PASSWORD = 1; // Use the improved version of Old Password Authentication
constexpr std::uint32_t CLIENT_FOUND_ROWS = 2; // Send found rows instead of affected rows in EOF_Packet
constexpr std::uint32_t CLIENT_LONG_FLAG = 4; // Get all column flags
constexpr std::uint32_t CLIENT_CONNECT_WITH_DB = 8; // Database (schema) name can be specified on connect in Handshake Response Packet
constexpr std::uint32_t CLIENT_NO_SCHEMA = 16; // Don't allow database.table.column
constexpr std::uint32_t CLIENT_COMPRESS = 32; // Compression protocol supported
constexpr std::uint32_t CLIENT_ODBC = 64; // Special handling of ODBC behavior
constexpr std::uint32_t CLIENT_LOCAL_FILES = 128; // Can use LOAD DATA LOCAL
constexpr std::uint32_t CLIENT_IGNORE_SPACE = 256; // Ignore spaces before '('
constexpr std::uint32_t CLIENT_PROTOCOL_41 = 512; // New 4.1 protocol
constexpr std::uint32_t CLIENT_INTERACTIVE = 1024; // This is an interactive client
constexpr std::uint32_t CLIENT_SSL = 2048; // Use SSL encryption for the session
constexpr std::uint32_t CLIENT_IGNORE_SIGPIPE = 4096; // Client only flag
constexpr std::uint32_t CLIENT_TRANSACTIONS = 8192; // Client knows about transactions
constexpr std::uint32_t CLIENT_RESERVED = 16384; // DEPRECATED: Old flag for 4.1 protocol
constexpr std::uint32_t CLIENT_RESERVED2 = 32768; // DEPRECATED: Old flag for 4.1 authentication \ CLIENT_SECURE_CONNECTION
constexpr std::uint32_t CLIENT_MULTI_STATEMENTS = (1UL << 16); // Enable/disable multi-stmt support
constexpr std::uint32_t CLIENT_MULTI_RESULTS = (1UL << 17); // Enable/disable multi-results
constexpr std::uint32_t CLIENT_PS_MULTI_RESULTS = (1UL << 18); // Multi-results and OUT parameters in PS-protocol
constexpr std::uint32_t CLIENT_PLUGIN_AUTH = (1UL << 19); // Client supports plugin authentication
constexpr std::uint32_t CLIENT_CONNECT_ATTRS = (1UL << 20); // Client supports connection attributes
constexpr std::uint32_t CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA = (1UL << 21); // Enable authentication response packet to be larger than 255 bytes
constexpr std::uint32_t CLIENT_CAN_HANDLE_EXPIRED_PASSWORDS = (1UL << 22); // Don't close the connection for a user account with expired password
constexpr std::uint32_t CLIENT_SESSION_TRACK = (1UL << 23); // Capable of handling server state change information
constexpr std::uint32_t CLIENT_DEPRECATE_EOF = (1UL << 24); // Client no longer needs EOF_Packet and will use OK_Packet instead
constexpr std::uint32_t CLIENT_SSL_VERIFY_SERVER_CERT = (1UL << 30); // Verify server certificate
constexpr std::uint32_t CLIENT_OPTIONAL_RESULTSET_METADATA = (1UL << 25); // The client can handle optional metadata information in the resultset
constexpr std::uint32_t CLIENT_REMEMBER_OPTIONS = (1UL << 31); // Don't reset the options after an unsuccessful connect

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

// Column definitions
enum class FieldType : std::uint8_t
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

// Prepared statements
constexpr std::uint8_t CURSOR_TYPE_NO_CURSOR = 0;
constexpr std::uint8_t CURSOR_TYPE_READ_ONLY = 1;
constexpr std::uint8_t CURSOR_TYPE_FOR_UPDATE = 2;
constexpr std::uint8_t CURSOR_TYPE_SCROLLABLE = 4;

}
}



#endif /* INCLUDE_MYSQL_IMPL_CONSTANTS_HPP_ */
