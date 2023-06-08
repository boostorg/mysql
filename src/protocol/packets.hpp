//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_SRC_PROTOCOL_PACKETS_HPP
#define BOOST_MYSQL_SRC_PROTOCOL_PACKETS_HPP

#include <boost/mysql/column_type.hpp>

#include <cstddef>

#include "protocol/protocol.hpp"
#include "protocol/serialization.hpp"

namespace boost {
namespace mysql {
namespace detail {

// ok_view
deserialize_errc deserialize(deserialization_context& ctx, ok_view& output) noexcept;

// Error packet. This is not exposed in the protocol interface
struct err_view
{
    std::uint16_t error_code;
    string_view error_message;
};
deserialize_errc deserialize(deserialization_context& ctx, err_view& pack) noexcept;

// Column definition
column_type compute_column_type(
    protocol_field_type protocol_type,
    std::uint16_t flags,
    std::uint16_t collation
) noexcept;  // exposed for the sake of testing
deserialize_errc deserialize(deserialization_context& ctx, coldef_view& output) noexcept;

// Quit
constexpr std::size_t get_size(quit_command) noexcept { return 1; }
inline void serialize(serialization_context& ctx, quit_command) noexcept
{
    constexpr std::uint8_t command_id = 0x01;
    serialize(ctx, command_id);
}

// Ping
constexpr std::size_t get_size(ping_command) noexcept { return 1; }
inline void serialize(serialization_context& ctx, ping_command) noexcept
{
    constexpr std::uint8_t command_id = 0x0e;
    serialize(ctx, command_id);
}

// Query
inline std::size_t get_size(query_command value) noexcept
{
    return get_size(string_eof{value.query}) + 1;  // command ID
}
inline void serialize(serialization_context& ctx, query_command value) noexcept
{
    constexpr std::uint8_t command_id = 3;
    serialize(ctx, command_id, string_eof(value.query));
}

// Prepare statement
inline std::size_t get_size(prepare_stmt_command value) noexcept
{
    return get_size(string_eof{value.stmt}) + 1;  // command ID
}
inline void serialize(serialization_context& ctx, prepare_stmt_command pack) noexcept
{
    constexpr std::uint8_t command_id = 0x16;
    serialize(ctx, command_id, string_eof{pack.stmt});
}

deserialize_errc deserialize(deserialization_context& ctx, prepare_stmt_response& output) noexcept;

// Execute statement
std::size_t get_size(execute_stmt_command value) noexcept;
void serialize(serialization_context& ctx, execute_stmt_command value) noexcept;

// Close statement
constexpr std::size_t get_size(close_stmt_command) noexcept { return 5; }
inline void serialize(serialization_context& ctx, close_stmt_command value) noexcept
{
    constexpr std::uint8_t command_id = 0x19;
    serialize(ctx, command_id, value.statement_id);
}

// server_hello
deserialize_errc deserialize(deserialization_context& ctx, server_hello& output) noexcept;

// login_request
std::size_t get_size(const login_request& value) noexcept;
void serialize(serialization_context& ctx, const login_request& value) noexcept;

// ssl_request
std::size_t get_size(ssl_request) noexcept;
void serialize(serialization_context& ctx, ssl_request value) noexcept;

// auth_switch
deserialize_errc deserialize(deserialization_context& ctx, auth_switch& output) noexcept;

// auth_switch_response
inline std::size_t get_size(auth_switch_response value) noexcept
{
    return get_size(string_eof{to_string(value.auth_plugin_data)});
}
inline void serialize(serialization_context& ctx, auth_switch_response value) noexcept
{
    serialize(ctx, string_eof{to_string(value.auth_plugin_data)});
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
