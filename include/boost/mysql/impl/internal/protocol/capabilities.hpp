//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_CAPABILITIES_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_CAPABILITIES_HPP

#include <boost/config.hpp>

#include <cstdint>

namespace boost {
namespace mysql {
namespace detail {

enum class capabilities : std::uint32_t
{
    // CLIENT_LONG_PASSWORD: Use the improved version of Old Password Authentication
    long_password = 1,

    // CLIENT_FOUND_ROWS: Send found rows instead of affected rows in EOF_Packet
    found_rows = 2,

    // CLIENT_LONG_FLAG: Get all column flags
    long_flag = 4,

    // CLIENT_CONNECT_WITH_DB: Database (schema) name can be specified on connect in Handshake Response Packet
    connect_with_db = 8,

    // CLIENT_NO_SCHEMA: Don't allow database.table.column
    no_schema = 16,

    // CLIENT_COMPRESS: Compression protocol supported
    compress = 32,

    // CLIENT_ODBC: Special handling of ODBC behavior
    odbc = 64,

    // CLIENT_LOCAL_FILES: Can use LOAD DATA LOCAL
    local_files = 128,

    // CLIENT_IGNORE_SPACE: Ignore spaces before '('
    ignore_space = 256,

    // CLIENT_PROTOCOL_41: New 4.1 protocol
    protocol_41 = 512,

    // CLIENT_INTERACTIVE: This is an interactive client
    interactive = 1024,

    // CLIENT_SSL: Use SSL encryption for the session
    ssl = 2048,

    // CLIENT_IGNORE_SIGPIPE: Client only flag
    ignore_sigpipe = 4096,

    // CLIENT_TRANSACTIONS: Client knows about transactions
    transactions = 8192,

    // CLIENT_RESERVED: DEPRECATED: Old flag for 4.1 protocol
    reserved = 16384,

    // CLIENT_SECURE_CONNECTION: DEPRECATED: Old flag for 4.1 authentication, required by MariaDB
    secure_connection = 32768,

    // CLIENT_MULTI_STATEMENTS: Enable/disable multi-stmt support
    multi_statements = (1UL << 16),

    // CLIENT_MULTI_RESULTS: Enable/disable multi-results
    multi_results = (1UL << 17),

    // CLIENT_PS_MULTI_RESULTS: Multi-results and OUT parameters in PS-protocol
    ps_multi_results = (1UL << 18),

    // CLIENT_PLUGIN_AUTH: Client supports plugin authentication
    plugin_auth = (1UL << 19),

    // CLIENT_CONNECT_ATTRS: Client supports connection attributes
    connect_attrs = (1UL << 20),

    // CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA: Enable authentication response packet to be larger than 255
    // bytes
    plugin_auth_lenenc_data = (1UL << 21),

    // CLIENT_CAN_HANDLE_EXPIRED_PASSWORDS: Don't close the connection for a user account with expired
    // password
    can_handle_expired_passwords = (1UL << 22),

    // CLIENT_SESSION_TRACK: Capable of handling server state change information
    session_track = (1UL << 23),

    // CLIENT_DEPRECATE_EOF: Client no longer needs EOF_Packet and will use OK_Packet instead
    deprecate_eof = (1UL << 24),

    // CLIENT_SSL_VERIFY_SERVER_CERT: Verify server certificate
    ssl_verify_server_cert = (1UL << 30),

    // CLIENT_OPTIONAL_RESULTSET_METADATA: The client can handle optional metadata information in the
    // resultset
    optional_resultset_metadata = (1UL << 25),

    // CLIENT_REMEMBER_OPTIONS: Don't reset the options after an unsuccessful connect
    remember_options = (1UL << 31),
};

constexpr capabilities operator&(capabilities lhs, capabilities rhs)
{
    return static_cast<capabilities>(static_cast<std::uint32_t>(lhs) & static_cast<std::uint32_t>(rhs));
}

constexpr capabilities operator|(capabilities lhs, capabilities rhs)
{
    return static_cast<capabilities>(static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs));
}

// Are all capabilities in subset in caps?
constexpr bool has_capabilities(capabilities caps, capabilities subset) { return (caps & subset) == subset; }

/*
 * CLIENT_LONG_PASSWORD: unset //  Use the improved version of Old Password Authentication
 * CLIENT_FOUND_ROWS: unset //  Send found rows instead of affected rows in EOF_Packet
 * CLIENT_LONG_FLAG: unset //  Get all column flags
 * CLIENT_CONNECT_WITH_DB: optional //  Database (schema) name can be specified on connect in
 * Handshake Response Packet CLIENT_NO_SCHEMA: unset //  Don't allow database.table.column
 * CLIENT_COMPRESS: unset //  Compression protocol supported
 * CLIENT_ODBC: unset //  Special handling of ODBC behavior
 * CLIENT_LOCAL_FILES: unset //  Can use LOAD DATA LOCAL
 * CLIENT_IGNORE_SPACE: unset //  Ignore spaces before '('
 * CLIENT_PROTOCOL_41: mandatory //  New 4.1 protocol
 * CLIENT_INTERACTIVE: unset //  This is an interactive client
 * CLIENT_SSL: unset //  Use SSL encryption for the session
 * CLIENT_IGNORE_SIGPIPE: unset //  Client only flag
 * CLIENT_TRANSACTIONS: unset //  Client knows about transactions
 * CLIENT_RESERVED: unset //  DEPRECATED: Old flag for 4.1 protocol
 * CLIENT_RESERVED2: unset //  DEPRECATED: Old flag for 4.1 authentication
 * \ CLIENT_SECURE_CONNECTION CLIENT_MULTI_STATEMENTS: unset //  Enable/disable multi-stmt support
 * CLIENT_MULTI_RESULTS: unset //  Enable/disable multi-results
 * CLIENT_PS_MULTI_RESULTS: unset //  Multi-results and OUT parameters in PS-protocol
 * CLIENT_PLUGIN_AUTH: mandatory //  Client supports plugin authentication
 * CLIENT_CONNECT_ATTRS: unset //  Client supports connection attributes
 * CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA: mandatory //  Enable authentication response packet to be
 * larger than 255 bytes CLIENT_CAN_HANDLE_EXPIRED_PASSWORDS: unset //  Don't close the connection
 * for a user account with expired password CLIENT_SESSION_TRACK: unset //  Capable of handling
 * server state change information CLIENT_DEPRECATE_EOF: mandatory //  Client no longer needs
 * EOF_Packet and will use OK_Packet instead CLIENT_SSL_VERIFY_SERVER_CERT: unset //  Verify server
 * certificate CLIENT_OPTIONAL_RESULTSET_METADATA: unset //  The client can handle optional metadata
 * information in the resultset CLIENT_REMEMBER_OPTIONS: unset //  Don't reset the options after an
 * unsuccessful connect
 *
 * We pay attention to:
 * CLIENT_CONNECT_WITH_DB: optional //  Database (schema) name can be specified on connect in
 * Handshake Response Packet CLIENT_PROTOCOL_41: mandatory //  New 4.1 protocol CLIENT_PLUGIN_AUTH:
 * mandatory //  Client supports plugin authentication CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA:
 * mandatory //  Enable authentication response packet to be larger than 255 bytes
 * CLIENT_DEPRECATE_EOF: mandatory //  Client no longer needs EOF_Packet and will use OK_Packet
 * instead
 */

// clang-format off
BOOST_INLINE_CONSTEXPR capabilities mandatory_capabilities = 
    capabilities::protocol_41 |
    capabilities::plugin_auth |
    capabilities::plugin_auth_lenenc_data |
    capabilities::deprecate_eof |
    capabilities::secure_connection
;
// clang-format on

BOOST_INLINE_CONSTEXPR capabilities optional_capabilities = capabilities::multi_results |
                                                            capabilities::ps_multi_results;

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
