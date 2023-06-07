//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_SRC_PROTOCOL_PROTOCOL_HPP
#define BOOST_MYSQL_SRC_PROTOCOL_PROTOCOL_HPP

#include <boost/mysql/column_type.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/metadata_collection_view.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/resultset_encoding.hpp>

#include <boost/asio/buffer.hpp>
#include <boost/config.hpp>
#include <boost/core/span.hpp>

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "protocol/capabilities.hpp"
#include "protocol/constants.hpp"
#include "protocol/db_flavor.hpp"
#include "protocol/static_string.hpp"

namespace boost {
namespace mysql {
namespace detail {

// Frame header
constexpr std::size_t frame_header_size = 4;

struct frame_header
{
    std::uint32_t size;
    std::uint8_t sequence_number;
};

void serialize_frame_header(frame_header, span<std::uint8_t, frame_header_size> buffer) noexcept;
frame_header deserialize_frame_header(span<const std::uint8_t, frame_header_size> buffer) noexcept;

// OK packets (views because strings are non-owning)
struct ok_view
{
    std::uint64_t affected_rows;
    std::uint64_t last_insert_id;
    std::uint16_t status_flags;
    std::uint16_t warnings;
    string_view info;

    bool more_results() const noexcept { return status_flags & SERVER_MORE_RESULTS_EXISTS; }
    bool is_out_params() const noexcept { return status_flags & SERVER_PS_OUT_PARAMS; }
};

// Column definition
struct coldef_view
{
    string_view database;
    string_view table;
    string_view org_table;
    string_view column_name;
    string_view org_column_name;
    std::uint16_t collation_id;
    std::uint32_t column_length;  // maximum length of the field
    column_type type;
    std::uint16_t flags;
    std::uint8_t decimals;  // max shown decimal digits. 0x00 for int/static strings; 0x1f for
                            // dynamic strings, double, float
};
BOOST_ATTRIBUTE_NODISCARD
error_code deserialize_column_definition(asio::const_buffer input, coldef_view& output) noexcept;

// Quit
struct quit_command
{
    std::size_t get_size() const noexcept;
    void serialize(asio::mutable_buffer) const noexcept;
};

// Ping
struct ping_command
{
    std::size_t get_size() const noexcept;
    void serialize(asio::mutable_buffer) const noexcept;
};
BOOST_ATTRIBUTE_NODISCARD
error_code deserialize_ping_response(asio::const_buffer message, db_flavor flavor, diagnostics& diag);

// Query
struct query_command
{
    string_view query;

    std::size_t get_size() const noexcept;
    void serialize(asio::mutable_buffer) const noexcept;
};

// Prepare statement
struct prepare_stmt_command
{
    string_view stmt;

    std::size_t get_size() const noexcept;
    void serialize(asio::mutable_buffer) const noexcept;
};
struct prepare_stmt_response
{
    std::uint32_t id;
    std::uint16_t num_columns;
    std::uint16_t num_params;
};
BOOST_ATTRIBUTE_NODISCARD
error_code deserialize_prepare_stmt_response(
    asio::const_buffer message,
    db_flavor flavor,
    prepare_stmt_response& output,
    diagnostics& diag
);

// Execute statement
struct execute_stmt_command
{
    std::uint32_t statement_id;
    span<const field_view> params;

    std::size_t get_size() const noexcept;
    void serialize(asio::mutable_buffer) const noexcept;
};

// Close statement
struct close_stmt_command
{
    std::uint32_t statement_id{};

    std::size_t get_size() const noexcept;
    void serialize(asio::mutable_buffer) const noexcept;
};

// Execution messages
static_assert(std::is_trivially_destructible<error_code>::value, "");
struct execute_response
{
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
execute_response deserialize_execute_response(
    asio::const_buffer msg,
    db_flavor flavor,
    diagnostics& diag
) noexcept;

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
row_message deserialize_row_message(asio::const_buffer msg, db_flavor flavor, diagnostics& diag);

error_code deserialize_row(
    resultset_encoding encoding,
    span<const std::uint8_t> message,
    metadata_collection_view meta,
    span<field_view> output  // Should point to meta.size() field_view objects
);

// Handshake messages
struct server_hello
{
    using auth_buffer_type = static_string<8 + 0xff>;
    db_flavor server;
    auth_buffer_type auth_plugin_data;
    capabilities server_capabilities{};
    string_view auth_plugin_name;
};
BOOST_ATTRIBUTE_NODISCARD
error_code deserialize_server_hello(asio::const_buffer msg, server_hello& output, diagnostics& diag);

struct login_request
{
    capabilities negotiated_capabilities;  // capabilities
    std::uint32_t max_packet_size;
    std::uint32_t collation_id;
    string_view username;
    span<const std::uint8_t> auth_response;
    string_view database;
    string_view auth_plugin_name;

    std::size_t get_size() const noexcept;
    void serialize(asio::mutable_buffer) const noexcept;
};

struct ssl_request
{
    capabilities negotiated_capabilities;
    std::uint32_t max_packet_size;
    std::uint32_t collation_id;

    std::size_t get_size() const noexcept;
    void serialize(asio::mutable_buffer) const noexcept;
};

struct auth_switch
{
    string_view plugin_name;
    span<const std::uint8_t> auth_data;
};

struct auth_switch_response
{
    span<const std::uint8_t> auth_plugin_data;

    std::size_t get_size() const noexcept;
    void serialize(asio::mutable_buffer) const noexcept;
};

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
handhake_server_response deserialize_handshake_server_response(
    asio::const_buffer buff,
    db_flavor flavor,
    diagnostics& diag
);

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
