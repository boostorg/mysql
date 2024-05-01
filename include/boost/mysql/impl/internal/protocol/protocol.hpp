//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_PROTOCOL_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_PROTOCOL_HPP

#include <boost/mysql/column_type.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/metadata_collection_view.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/coldef_view.hpp>
#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/ok_view.hpp>
#include <boost/mysql/detail/resultset_encoding.hpp>

#include <boost/mysql/impl/internal/protocol/basic_types.hpp>
#include <boost/mysql/impl/internal/protocol/capabilities.hpp>
#include <boost/mysql/impl/internal/protocol/constants.hpp>
#include <boost/mysql/impl/internal/protocol/db_flavor.hpp>
#include <boost/mysql/impl/internal/protocol/serialization.hpp>
#include <boost/mysql/impl/internal/protocol/static_buffer.hpp>

#include <boost/config.hpp>
#include <boost/core/span.hpp>

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace boost {
namespace mysql {
namespace detail {

//
// Serialization
//

// quit
struct quit_command
{
};
inline void serialize(serialization_context& ctx, quit_command) { ctx.add(0x01); }

// ping
struct ping_command
{
};
inline void serialize(serialization_context& ctx, ping_command) { ctx.add(0x0e); }

// reset_connection
struct reset_connection_command
{
};
inline void serialize(serialization_context& ctx, reset_connection_command) { ctx.add(0x1f); }

// query
struct query_command
{
    string_view query;
};
inline void serialize(serialization_context& ctx, query_command cmd)
{
    ctx.add(0x03);
    serialize_checked(ctx, string_eof{cmd.query});
}

// prepare_statement
struct prepare_stmt_command
{
    string_view stmt;
};
inline void serialize(serialization_context& ctx, prepare_stmt_command cmd)
{
    ctx.add(0x16);
    serialize(ctx, string_eof{cmd.stmt});
}

// execute statement
struct execute_stmt_command
{
    std::uint32_t statement_id;
    span<const field_view> params;
};
inline void serialize(serialization_context& ctx, execute_stmt_command cmd);

// close statement
struct close_stmt_command
{
    std::uint32_t statement_id;
};
inline void serialize(serialization_context& ctx, close_stmt_command cmd)
{
    ctx.add(0x19);
    serialize(ctx, cmd.statement_id);
}

// Login request
struct login_request
{
    capabilities negotiated_capabilities;  // capabilities
    std::uint32_t max_packet_size;
    std::uint32_t collation_id;
    string_view username;
    span<const std::uint8_t> auth_response;
    string_view database;
    string_view auth_plugin_name;
};
inline void serialize(serialization_context& ctx, const login_request&);

// SSL request
struct ssl_request
{
    capabilities negotiated_capabilities;
    std::uint32_t max_packet_size;
    std::uint32_t collation_id;
};
inline void serialize(serialization_context& ctx, ssl_request);

// Auth switch response
struct auth_switch_response
{
    span<const std::uint8_t> auth_plugin_data;
};
inline void serialize(serialization_context& ctx, auth_switch_response cmd) { ctx.add(cmd.auth_plugin_data); }

// Serialize a complete message
template <class Serializable>
inline std::uint8_t serialize_top_level(
    const Serializable& input,
    std::vector<std::uint8_t>& to,
    std::uint8_t seqnum,
    std::size_t max_frame_size = max_packet_size
)
{
    // Create a serialization context
    serialization_context ctx(to, max_frame_size);

    // Serialize the object
    serialize(ctx, input);

    // Correctly set up frame headers
    return ctx.write_frame_headers(seqnum);
}

//
// Deserialization
//

// Frame header
struct frame_header
{
    std::uint32_t size;
    std::uint8_t sequence_number;
};
inline frame_header deserialize_frame_header(span<const std::uint8_t, frame_header_size> buffer);

// OK packets (views because strings are non-owning)
inline error_code deserialize_ok_packet(span<const std::uint8_t> msg, ok_view& output);  // for testing

// Error packets (exposed for testing)
struct err_view
{
    std::uint16_t error_code;
    string_view error_message;
};
BOOST_ATTRIBUTE_NODISCARD inline error_code deserialize_error_packet(
    span<const std::uint8_t> message,
    err_view& pack,
    bool has_sql_state = true
);
BOOST_ATTRIBUTE_NODISCARD inline error_code process_error_packet(
    span<const std::uint8_t> message,
    db_flavor flavor,
    diagnostics& diag,
    bool has_sql_state = true
);

// Deserializes a response that may be an OK or an error packet.
// Applicable for commands like ping and reset connection.
// If the response is an OK packet, sets backslash_escapes according to the
// OK packet's server status flags
BOOST_ATTRIBUTE_NODISCARD inline error_code deserialize_ok_response(
    span<const std::uint8_t> message,
    db_flavor flavor,
    diagnostics& diag,
    bool& backslash_escapes
);

// Column definition
BOOST_ATTRIBUTE_NODISCARD inline error_code deserialize_column_definition(
    span<const std::uint8_t> input,
    coldef_view& output
);

// Prepare statement response
struct prepare_stmt_response
{
    std::uint32_t id;
    std::uint16_t num_columns;
    std::uint16_t num_params;
};
BOOST_ATTRIBUTE_NODISCARD inline error_code deserialize_prepare_stmt_response_impl(
    span<const std::uint8_t> message,
    prepare_stmt_response& output
);  // exposed for testing, doesn't take header into account
BOOST_ATTRIBUTE_NODISCARD inline error_code deserialize_prepare_stmt_response(
    span<const std::uint8_t> message,
    db_flavor flavor,
    prepare_stmt_response& output,
    diagnostics& diag
);

// Execution messages
struct execute_response
{
    static_assert(std::is_trivially_destructible<error_code>::value, "");

    enum class type_t
    {
        num_fields,
        ok_packet,
        error
    } type;
    union data_t
    {
        std::size_t num_fields;
        ok_view ok_pack;
        error_code err;

        data_t(size_t v) noexcept : num_fields(v) {}
        data_t(const ok_view& v) noexcept : ok_pack(v) {}
        data_t(error_code v) noexcept : err(v) {}
    } data;

    execute_response(std::size_t v) noexcept : type(type_t::num_fields), data(v) {}
    execute_response(const ok_view& v) noexcept : type(type_t::ok_packet), data(v) {}
    execute_response(error_code v) noexcept : type(type_t::error), data(v) {}
};
inline execute_response deserialize_execute_response(
    span<const std::uint8_t> msg,
    db_flavor flavor,
    diagnostics& diag
);

struct row_message
{
    enum class type_t
    {
        row,
        ok_packet,
        error
    } type;
    union data_t
    {
        span<const std::uint8_t> row;
        ok_view ok_pack;
        error_code err;

        data_t(span<const std::uint8_t> row) noexcept : row(row) {}
        data_t(const ok_view& ok_pack) noexcept : ok_pack(ok_pack) {}
        data_t(error_code err) noexcept : err(err) {}
    } data;

    row_message(span<const std::uint8_t> row) noexcept : type(type_t::row), data(row) {}
    row_message(const ok_view& ok_pack) noexcept : type(type_t::ok_packet), data(ok_pack) {}
    row_message(error_code v) noexcept : type(type_t::error), data(v) {}
};
inline row_message deserialize_row_message(span<const std::uint8_t> msg, db_flavor flavor, diagnostics& diag);

inline error_code deserialize_row(
    resultset_encoding encoding,
    span<const std::uint8_t> message,
    metadata_collection_view meta,
    span<field_view> output  // Should point to meta.size() field_view objects
);

// Server hello
struct server_hello
{
    using auth_buffer_type = static_buffer<8 + 0xff>;
    db_flavor server;
    auth_buffer_type auth_plugin_data;
    capabilities server_capabilities{};
    string_view auth_plugin_name;
};
BOOST_ATTRIBUTE_NODISCARD inline error_code deserialize_server_hello_impl(
    span<const std::uint8_t> msg,
    server_hello& output
);  // exposed for testing, doesn't take message header into account
BOOST_ATTRIBUTE_NODISCARD inline error_code deserialize_server_hello(
    span<const std::uint8_t> msg,
    server_hello& output,
    diagnostics& diag
);

// Auth switch
struct auth_switch
{
    string_view plugin_name;
    span<const std::uint8_t> auth_data;
};
BOOST_ATTRIBUTE_NODISCARD inline error_code deserialize_auth_switch(
    span<const std::uint8_t> msg,
    auth_switch& output
);  // exposed for testing

// Handshake server response
struct handhake_server_response
{
    struct ok_follows_t
    {
    };

    enum class type_t
    {
        ok,
        error,
        ok_follows,
        auth_switch,
        auth_more_data
    } type;

    union data_t
    {
        ok_view ok;
        error_code err;
        ok_follows_t ok_follows;
        auth_switch auth_sw;
        span<const std::uint8_t> more_data;

        data_t(const ok_view& ok) noexcept : ok(ok) {}
        data_t(error_code err) noexcept : err(err) {}
        data_t(ok_follows_t) noexcept : ok_follows({}) {}
        data_t(auth_switch msg) noexcept : auth_sw(msg) {}
        data_t(span<const std::uint8_t> more_data) noexcept : more_data(more_data) {}
    } data;

    handhake_server_response(const ok_view& ok) noexcept : type(type_t::ok), data(ok) {}
    handhake_server_response(error_code err) noexcept : type(type_t::error), data(err) {}
    handhake_server_response(ok_follows_t) noexcept : type(type_t::ok_follows), data(ok_follows_t{}) {}
    handhake_server_response(auth_switch auth_switch) noexcept : type(type_t::auth_switch), data(auth_switch)
    {
    }
    handhake_server_response(span<const std::uint8_t> more_data) noexcept
        : type(type_t::auth_more_data), data(more_data)
    {
    }
};
inline handhake_server_response deserialize_handshake_server_response(
    span<const std::uint8_t> buff,
    db_flavor flavor,
    diagnostics& diag
);

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#include <boost/mysql/impl/internal/protocol/protocol_impl.hpp>

#endif
