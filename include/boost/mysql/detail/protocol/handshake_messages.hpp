#ifndef INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_HANDSHAKE_MESSAGES_HPP_
#define INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_HANDSHAKE_MESSAGES_HPP_

#include "boost/mysql/detail/protocol/serialization.hpp"

namespace boost {
namespace mysql {
namespace detail {

struct handshake_packet
{
	// int<1> 	protocol version 	Always 10
	string_null server_version;
	int4 connection_id;
	string_lenenc auth_plugin_data; // not an actual protocol field, the merge of two fields
	int4 capability_falgs; // merge of the two parts - not an actual field
	int1 character_set; // default server a_protocol_character_set, only the lower 8-bits
	int2 status_flags; // server_status_flags
	string_null auth_plugin_name;

	std::array<char, 8 + 0xff> auth_plugin_data_buffer; // not an actual protocol field, the merge of two fields
};

template <>
struct get_struct_fields<handshake_packet>
{
	static constexpr auto value = std::make_tuple(
		&handshake_packet::server_version,
		&handshake_packet::connection_id,
		&handshake_packet::auth_plugin_data,
		&handshake_packet::capability_falgs,
		&handshake_packet::character_set,
		&handshake_packet::status_flags,
		&handshake_packet::auth_plugin_name
	);
};

struct handshake_response_packet
{
	int4 client_flag; // capabilities
	int4 max_packet_size;
	int1 character_set;
	// string[23] 	filler 	filler to the size of the handhshake response packet. All 0s.
	string_null username;
	string_lenenc auth_response; // we require CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA
	string_null database; // only to be serialized if CLIENT_CONNECT_WITH_DB
	string_null client_plugin_name; // we require CLIENT_PLUGIN_AUTH
	// TODO: CLIENT_CONNECT_ATTRS
};

template <>
struct get_struct_fields<handshake_response_packet>
{
	static constexpr auto value = std::make_tuple(
		&handshake_response_packet::client_flag,
		&handshake_response_packet::max_packet_size,
		&handshake_response_packet::character_set,
		&handshake_response_packet::username,
		&handshake_response_packet::auth_response,
		&handshake_response_packet::database,
		&handshake_response_packet::client_plugin_name
	);
};

struct auth_switch_request_packet
{
	string_null plugin_name;
	string_eof auth_plugin_data;
};

template <>
struct get_struct_fields<auth_switch_request_packet>
{
	static constexpr auto value = std::make_tuple(
		&auth_switch_request_packet::plugin_name,
		&auth_switch_request_packet::auth_plugin_data
	);
};

struct auth_switch_response_packet
{
	string_eof auth_plugin_data;
};

template <>
struct get_struct_fields<auth_switch_response_packet>
{
	static constexpr auto value = std::make_tuple(
		&auth_switch_response_packet::auth_plugin_data
	);
};

template <>
struct serialization_traits<handshake_packet, struct_tag> : noop_serialization_traits
{
	static inline errc deserialize_(handshake_packet& output, deserialization_context& ctx) noexcept;
};

template <>
struct serialization_traits<handshake_response_packet, struct_tag> : noop_serialization_traits
{
	static inline std::size_t get_size_(const handshake_response_packet& value, const serialization_context& ctx) noexcept;
	static inline void serialize_(const handshake_response_packet& value, serialization_context& ctx) noexcept;
};

template <>
struct serialization_traits<auth_switch_request_packet, struct_tag> : noop_serialization_traits
{
	static inline errc deserialize_(auth_switch_request_packet& output, deserialization_context& ctx) noexcept;
};


} // detail
} // mysql
} // boost

#include "boost/mysql/detail/protocol/impl/handshake_messages.ipp"

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_HANDSHAKE_MESSAGES_HPP_ */
