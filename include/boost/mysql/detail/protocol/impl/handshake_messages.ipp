//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_HANDSHAKE_MESSAGES_IPP
#define BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_HANDSHAKE_MESSAGES_IPP


inline boost::mysql::errc
boost::mysql::detail::serialization_traits<
    boost::mysql::detail::handshake_packet,
    boost::mysql::detail::serialization_tag::struct_with_fields
>::deserialize_(
    handshake_packet& output,
    deserialization_context& ctx
) noexcept
{
    constexpr std::uint8_t auth1_length = 8;
    string_fixed<auth1_length> auth_plugin_data_part_1;
    string_fixed<2> capability_flags_low;
    string_fixed<2> capability_flags_high;
    int1 filler; // should be 0
    int1 auth_plugin_data_len;
    string_fixed<10> reserved;

    auto err = deserialize_fields(
        ctx,
        output.server_version,
        output.connection_id,
        auth_plugin_data_part_1,
        filler,
        capability_flags_low,
        output.character_set,
        output.status_flags,
        capability_flags_high
    );
    if (err != errc::ok) return err;

    // Compose capabilities
    auto capabilities_begin = reinterpret_cast<std::uint8_t*>(&output.capability_falgs.value);
    memcpy(capabilities_begin, capability_flags_low.data(), 2);
    memcpy(capabilities_begin + 2, capability_flags_high.data(), 2);
    boost::endian::little_to_native_inplace(output.capability_falgs.value);

    // Check minimum server capabilities to deserialize this frame
    capabilities cap (output.capability_falgs.value);
    if (!cap.has(CLIENT_PLUGIN_AUTH)) return errc::server_unsupported;

    // Deserialize following fields
    err = deserialize_fields(
        ctx,
        auth_plugin_data_len,
        reserved
    );
    if (err != errc::ok) return err;

    // Auth plugin data, second part
    auto auth2_length = static_cast<std::uint8_t>(
        std::max(13, auth_plugin_data_len.value - auth1_length));
    const void* auth2_data = ctx.first();
    if (!ctx.enough_size(auth2_length)) return errc::incomplete_message;
    ctx.advance(auth2_length);

    // Auth plugin name
    err = deserialize(output.auth_plugin_name, ctx);
    if (err != errc::ok) return err;

    // Compose auth_plugin_data
    output.auth_plugin_data.clear();
    output.auth_plugin_data.append(auth_plugin_data_part_1.data(), auth1_length);
    output.auth_plugin_data.append(auth2_data, auth2_length - 1); // discard an extra trailing NULL byte

    return errc::ok;
}

std::size_t
boost::mysql::detail::serialization_traits<
    boost::mysql::detail::handshake_response_packet,
    boost::mysql::detail::serialization_tag::struct_with_fields
>::get_size_(
    const handshake_response_packet& value,
    const serialization_context& ctx
) noexcept
{
    std::size_t res =
        get_size(value.client_flag, ctx) +
        get_size(value.max_packet_size, ctx) +
        get_size(value.character_set, ctx) +
        23 + // filler
        get_size(value.username, ctx) +
        get_size(value.auth_response, ctx);
    if (ctx.get_capabilities().has(CLIENT_CONNECT_WITH_DB))
    {
        res += get_size(value.database, ctx);
    }
    res += get_size(value.client_plugin_name, ctx);
    return res;
}

inline void
boost::mysql::detail::serialization_traits<
    boost::mysql::detail::handshake_response_packet,
    boost::mysql::detail::serialization_tag::struct_with_fields
>::serialize_(
    const handshake_response_packet& value,
    serialization_context& ctx
) noexcept
{
    serialize(value.client_flag, ctx);
    serialize(value.max_packet_size, ctx);
    serialize(value.character_set, ctx);
    std::uint8_t buffer [23] {};
    ctx.write(buffer, sizeof(buffer));
    serialize(value.username, ctx);
    serialize(value.auth_response, ctx);
    if (ctx.get_capabilities().has(CLIENT_CONNECT_WITH_DB))
    {
        serialize(value.database, ctx);
    }
    serialize(value.client_plugin_name, ctx);
}

inline boost::mysql::errc
boost::mysql::detail::serialization_traits<
    boost::mysql::detail::auth_switch_request_packet,
    boost::mysql::detail::serialization_tag::struct_with_fields
>::deserialize_(
    auth_switch_request_packet& output,
    deserialization_context& ctx
) noexcept
{
    auto err = deserialize_fields(ctx, output.plugin_name, output.auth_plugin_data);
    auto& auth_data = output.auth_plugin_data.value;
    // Discard an additional NULL at the end of auth data
    if (!auth_data.empty())
    {
        assert(auth_data.back() == 0);
        auth_data = auth_data.substr(0, auth_data.size() - 1);
    }
    return err;
}



#endif /* INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_HANDSHAKE_MESSAGES_IPP_ */
