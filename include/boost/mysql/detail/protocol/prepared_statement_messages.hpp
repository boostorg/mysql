#ifndef INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_PREPARED_STATEMENT_MESSAGES_HPP_
#define INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_PREPARED_STATEMENT_MESSAGES_HPP_

#include "boost/mysql/detail/protocol/protocol_types.hpp"
#include "boost/mysql/detail/protocol/serialization_context.hpp"
#include "boost/mysql/detail/protocol/deserialization_context.hpp"
#include "boost/mysql/detail/protocol/constants.hpp"
#include "boost/mysql/detail/auxiliar/get_struct_fields.hpp"
#include "boost/mysql/value.hpp"
#include "boost/mysql/error.hpp"

namespace boost {
namespace mysql {
namespace detail {


// Commands (prepared statements)
struct com_stmt_prepare_packet
{
	string_eof statement;

	static constexpr std::uint8_t command_id = 0x16;
};

template <>
struct get_struct_fields<com_stmt_prepare_packet>
{
	static constexpr auto value = std::make_tuple(
		&com_stmt_prepare_packet::statement
	);
};

struct com_stmt_prepare_ok_packet
{
	// int1 status: must be 0
	int4 statement_id;
	int2 num_columns;
	int2 num_params;
	// int1 reserved_1: must be 0
	int2 warning_count;
	// TODO: int1 metadata_follows when CLIENT_OPTIONAL_RESULTSET_METADATA
};

template <>
struct get_struct_fields<com_stmt_prepare_ok_packet>
{
	static constexpr auto value = std::make_tuple(
		&com_stmt_prepare_ok_packet::statement_id,
		&com_stmt_prepare_ok_packet::num_columns,
		&com_stmt_prepare_ok_packet::num_params,
		&com_stmt_prepare_ok_packet::warning_count
	);
};

template <typename ForwardIterator>
struct com_stmt_execute_packet
{
	int4 statement_id;
	int1 flags;
	int4 iteration_count;
	// if num_params > 0: NULL bitmap
	int1 new_params_bind_flag;
	ForwardIterator params_begin;
	ForwardIterator params_end;

	static constexpr std::uint8_t command_id = 0x17;
};

struct com_stmt_execute_param_meta_packet
{
	protocol_field_type type;
	int1 unsigned_flag;
};

template <typename ForwardIterator>
struct get_struct_fields<com_stmt_execute_packet<ForwardIterator>>
{
	static constexpr auto value = std::make_tuple(
		&com_stmt_execute_packet<ForwardIterator>::statement_id,
		&com_stmt_execute_packet<ForwardIterator>::flags,
		&com_stmt_execute_packet<ForwardIterator>::iteration_count,
		&com_stmt_execute_packet<ForwardIterator>::new_params_bind_flag
	);
};

template <>
struct get_struct_fields<com_stmt_execute_param_meta_packet>
{
	static constexpr auto value = std::make_tuple(
		&com_stmt_execute_param_meta_packet::type,
		&com_stmt_execute_param_meta_packet::unsigned_flag
	);
};

struct com_stmt_close_packet
{
	int4 statement_id;

	static constexpr std::uint8_t command_id = 0x19;
};

template <>
struct get_struct_fields<com_stmt_close_packet>
{
	static constexpr auto value = std::make_tuple(
		&com_stmt_close_packet::statement_id
	);
};

inline errc deserialize(com_stmt_prepare_ok_packet& output, deserialization_context& ctx) noexcept;

template <typename FowardIterator>
inline std::size_t get_size(const com_stmt_execute_packet<FowardIterator>& value, const serialization_context& ctx) noexcept;

template <typename FowardIterator>
inline void serialize(const com_stmt_execute_packet<FowardIterator>& input, serialization_context& ctx) noexcept;

} // detail
} // mysql
} // boost

#include "boost/mysql/detail/protocol/impl/prepared_statement_messages.hpp"

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_PREPARED_STATEMENT_MESSAGES_HPP_ */
