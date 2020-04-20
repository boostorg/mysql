//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PROTOCOL_COMMON_MESSAGES_HPP
#define BOOST_MYSQL_DETAIL_PROTOCOL_COMMON_MESSAGES_HPP

#include "boost/mysql/detail/protocol/serialization.hpp"
#include "boost/mysql/detail/protocol/constants.hpp"
#include "boost/mysql/collation.hpp"
#include <tuple>

namespace boost {
namespace mysql {
namespace detail {

// header
struct packet_header
{
	int3 packet_size;
	int1 sequence_number;
};

template <>
struct get_struct_fields<packet_header>
{
	static constexpr auto value = std::make_tuple(
		&packet_header::packet_size,
		&packet_header::sequence_number
	);
};

// ok packet
struct ok_packet
{
	// header: int<1> 	header 	0x00 or 0xFE the OK packet header
	int_lenenc affected_rows;
	int_lenenc last_insert_id;
	int2 status_flags; // server_status_flags
	int2 warnings;
	// TODO: CLIENT_SESSION_TRACK
	string_lenenc info;
};

template <>
struct get_struct_fields<ok_packet>
{
	static constexpr auto value = std::make_tuple(
		&ok_packet::affected_rows,
		&ok_packet::last_insert_id,
		&ok_packet::status_flags,
		&ok_packet::warnings,
		&ok_packet::info
	);
};

template <>
struct serialization_traits<ok_packet, serialization_tag::struct_with_fields> :
	noop_serialize<ok_packet>
{
	static inline errc deserialize_(ok_packet& output, deserialization_context& ctx) noexcept;
};

// err packet
struct err_packet
{
	// int<1> 	header 	0xFF ERR packet header
	int2 error_code;
	string_fixed<1> sql_state_marker;
	string_fixed<5> sql_state;
	string_eof error_message;
};

template <>
struct get_struct_fields<err_packet>
{
	static constexpr auto value = std::make_tuple(
		&err_packet::error_code,
		&err_packet::sql_state_marker,
		&err_packet::sql_state,
		&err_packet::error_message
	);
};

// col def
struct column_definition_packet
{
	string_lenenc catalog; // always "def"
	string_lenenc schema;
	string_lenenc table; // virtual table
	string_lenenc org_table; // physical table
	string_lenenc name; // virtual column name
	string_lenenc org_name; // physical column name
	collation character_set;
	int4 column_length; // maximum length of the field
	protocol_field_type type; // type of the column as defined in enum_field_types
	int2 flags; // Flags as defined in Column Definition Flags
	int1 decimals; // max shown decimal digits. 0x00 for int/static strings; 0x1f for dynamic strings, double, float
};

template <>
struct get_struct_fields<column_definition_packet>
{
	static constexpr auto value = std::make_tuple(
		&column_definition_packet::catalog,
		&column_definition_packet::schema,
		&column_definition_packet::table,
		&column_definition_packet::org_table,
		&column_definition_packet::name,
		&column_definition_packet::org_name,
		&column_definition_packet::character_set,
		&column_definition_packet::column_length,
		&column_definition_packet::type,
		&column_definition_packet::flags,
		&column_definition_packet::decimals
	);
};

template <>
struct serialization_traits<column_definition_packet, serialization_tag::struct_with_fields> :
	noop_serialize<column_definition_packet>
{
	static inline errc deserialize_(column_definition_packet& output, deserialization_context& ctx) noexcept;
};

// aux
inline error_code process_error_packet(deserialization_context& ctx, error_info& info);


} // detail
} // mysql
} // boost

#include "boost/mysql/detail/protocol/impl/common_messages.ipp"

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_COMMON_MESSAGES_HPP_ */
