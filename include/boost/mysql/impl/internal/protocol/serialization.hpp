//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_SERIALIZATION_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_SERIALIZATION_HPP

#include <boost/mysql/field_view.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/impl/internal/protocol/capabilities.hpp>
#include <boost/mysql/impl/internal/protocol/frame_header.hpp>
#include <boost/mysql/impl/internal/protocol/impl/binary_protocol.hpp>
#include <boost/mysql/impl/internal/protocol/impl/null_bitmap.hpp>
#include <boost/mysql/impl/internal/protocol/impl/protocol_field_type.hpp>
#include <boost/mysql/impl/internal/protocol/impl/protocol_types.hpp>
#include <boost/mysql/impl/internal/protocol/impl/serialization_context.hpp>

#include <boost/assert.hpp>

#include <cstdint>

namespace boost {
namespace mysql {
namespace detail {

// quit
struct quit_command
{
    void serialize(serialization_context& ctx) const { ctx.add(0x01); }
};

// ping
struct ping_command
{
    void serialize(serialization_context& ctx) const { ctx.add(0x0e); }
};

// reset_connection
struct reset_connection_command
{
    void serialize(serialization_context& ctx) const { ctx.add(0x1f); }
};

// query
struct query_command
{
    string_view query;

    void serialize(serialization_context& ctx) const
    {
        ctx.add(0x03);
        string_eof{query}.serialize_checked(ctx);
    }
};

// prepare_statement
struct prepare_stmt_command
{
    string_view stmt;

    void serialize(serialization_context& ctx) const
    {
        ctx.add(0x16);
        string_eof{stmt}.serialize(ctx);
    }
};

// execute statement
struct execute_stmt_command
{
    std::uint32_t statement_id;
    span<const field_view> params;

    inline void serialize(serialization_context& ctx) const;
};

// close statement
struct close_stmt_command
{
    std::uint32_t statement_id;
    void serialize(serialization_context& ctx) const
    {
        ctx.add(0x19);
        int4{statement_id}.serialize(ctx);
    }
};

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

    inline void serialize(serialization_context& ctx) const;
};

// SSL request
struct ssl_request
{
    capabilities negotiated_capabilities;
    std::uint32_t max_packet_size;
    std::uint32_t collation_id;

    inline void serialize(serialization_context& ctx) const;
};

// Auth switch response
struct auth_switch_response
{
    span<const std::uint8_t> auth_plugin_data;

    void serialize(serialization_context& ctx) const { ctx.add(auth_plugin_data); }
};

// Serialize a complete message
template <class Serializable>
inline std::uint8_t serialize_top_level(
    const Serializable& input,
    std::vector<std::uint8_t>& to,
    std::uint8_t seqnum = 0,
    std::size_t frame_size = max_packet_size
)
{
    serialization_context ctx(to, frame_size);
    input.serialize(ctx);
    return ctx.write_frame_headers(seqnum);
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

//
// Implementations
//

namespace boost {
namespace mysql {
namespace detail {

// Maps from an actual value to a protocol_field_type (for execute statement)
inline protocol_field_type to_protocol_field_type(field_kind kind)
{
    switch (kind)
    {
    case field_kind::null: return protocol_field_type::null;
    case field_kind::int64: return protocol_field_type::longlong;
    case field_kind::uint64: return protocol_field_type::longlong;
    case field_kind::string: return protocol_field_type::string;
    case field_kind::blob: return protocol_field_type::blob;
    case field_kind::float_: return protocol_field_type::float_;
    case field_kind::double_: return protocol_field_type::double_;
    case field_kind::date: return protocol_field_type::date;
    case field_kind::datetime: return protocol_field_type::datetime;
    case field_kind::time: return protocol_field_type::time;
    default: BOOST_ASSERT(false); return protocol_field_type::null;
    }
}

// Returns the collation ID's first byte (for login packets)
inline std::uint8_t get_collation_first_byte(std::uint32_t collation_id)
{
    return static_cast<std::uint8_t>(collation_id % 0xff);
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

void boost::mysql::detail::execute_stmt_command::serialize(serialization_context& ctx) const
{
    // The wire layout is as follows:
    //  command ID
    //  std::uint32_t statement_id;
    //  std::uint8_t flags;
    //  std::uint32_t iteration_count;
    //  if num_params > 0:
    //      NULL bitmap
    //      std::uint8_t new_params_bind_flag;
    //      array<meta_packet, num_params> meta;
    //          protocol_field_type type;
    //          std::uint8_t unsigned_flag;
    //      array<field_view, num_params> params;

    constexpr int1 command_id{0x17};
    constexpr int1 flags{0};
    constexpr int4 iteration_count{1};
    constexpr int1 new_params_bind_flag{1};

    // header
    ctx.serialize(command_id, int4{statement_id}, flags, iteration_count);

    // Number of parameters
    auto num_params = params.size();

    if (num_params > 0)
    {
        // NULL bitmap
        null_bitmap_generator null_gen(params);
        while (!null_gen.done())
            ctx.add(null_gen.next());

        // new parameters bind flag
        new_params_bind_flag.serialize(ctx);

        // value metadata
        for (field_view param : params)
        {
            field_kind kind = param.kind();
            protocol_field_type type = to_protocol_field_type(kind);
            std::uint8_t unsigned_flag = kind == field_kind::uint64 ? std::uint8_t(0x80) : std::uint8_t(0);
            ctx.add(static_cast<std::uint8_t>(type));
            ctx.add(unsigned_flag);
        }

        // actual values
        for (field_view param : params)
        {
            serialize_binary_field(ctx, param);
        }
    }
}

void boost::mysql::detail::login_request::serialize(serialization_context& ctx) const
{
    ctx.serialize(
        int4{negotiated_capabilities.get()},           // client_flag
        int4{max_packet_size},                         // max_packet_size
        int1{get_collation_first_byte(collation_id)},  //  character_set
        string_fixed<23>{},                            // filler (all zeros)
        string_null{username},
        string_lenenc{to_string(auth_response)}  // we require CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA
    );
    if (negotiated_capabilities.has(CLIENT_CONNECT_WITH_DB))
    {
        string_null{database}.serialize(ctx);  // database
    }
    string_null{auth_plugin_name}.serialize(ctx);  //  client_plugin_name
}

void boost::mysql::detail::ssl_request::serialize(serialization_context& ctx) const
{
    ctx.serialize(
        int4{negotiated_capabilities.get()},           // client_flag
        int4{max_packet_size},                         // max_packet_size
        int1{get_collation_first_byte(collation_id)},  // character_set,
        string_fixed<23>{}                             // filler, all zeros
    );
}

#endif
