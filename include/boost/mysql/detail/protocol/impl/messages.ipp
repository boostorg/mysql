#ifndef MYSQL_ASIO_IMPL_MESSAGES_IPP
#define MYSQL_ASIO_IMPL_MESSAGES_IPP

#include "boost/mysql/detail/protocol/serialization.hpp"
#include "boost/mysql/detail/protocol/null_bitmap_traits.hpp"
#include "boost/mysql/detail/protocol/binary_serialization.hpp"
#include <cassert>
#include <iterator>

namespace boost {
namespace mysql {
namespace detail {

// Maps from an actual value to a protocol_field_type. Only value's type is used
inline protocol_field_type get_protocol_field_type(
	const value& input
) noexcept
{
	struct visitor
	{
		constexpr auto operator()(std::int32_t) const noexcept { return protocol_field_type::long_; }
		constexpr auto operator()(std::uint32_t) const noexcept { return protocol_field_type::long_; }
		constexpr auto operator()(std::int64_t) const noexcept { return protocol_field_type::longlong; }
		constexpr auto operator()(std::uint64_t) const noexcept { return protocol_field_type::longlong; }
		constexpr auto operator()(std::string_view) const noexcept { return protocol_field_type::varchar; }
		constexpr auto operator()(float) const noexcept { return protocol_field_type::float_; }
		constexpr auto operator()(double) const noexcept { return protocol_field_type::double_; }
		constexpr auto operator()(date) const noexcept { return protocol_field_type::date; }
		constexpr auto operator()(datetime) const noexcept { return protocol_field_type::datetime; }
		constexpr auto operator()(time) const noexcept { return protocol_field_type::time; }
		constexpr auto operator()(std::nullptr_t) const noexcept { return protocol_field_type::null; }
	};
	return std::visit(visitor(), input);
}

// Whether to include the unsigned flag in the statement execute message
// for a given value or not. Only value's type is used
inline bool is_unsigned(
	const value& input
) noexcept
{
	// By default, return false; just for integer types explicitly unsigned return true
	return std::visit([](auto v) {
		using type = decltype(v);
		return std::is_same_v<type, std::uint32_t> ||
			   std::is_same_v<type, std::uint64_t>;
	}, input);
}

} // detail
} // mysql
} // boost

inline boost::mysql::Error boost::mysql::detail::deserialize(
	ok_packet& output,
	DeserializationContext& ctx
) noexcept
{
	auto err = deserialize_fields(
		ctx,
		output.affected_rows,
		output.last_insert_id,
		output.status_flags,
		output.warnings
	);
	if (err == Error::ok && ctx.enough_size(1)) // message is optional, may be omitted
	{
		err = deserialize(output.info, ctx);
	}
	return err;
}

inline boost::mysql::Error boost::mysql::detail::deserialize(
	handshake_packet& output,
	DeserializationContext& ctx
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
		filler, // TODO: docs state fields below the filler are optional
		capability_flags_low,
		output.character_set,
		output.status_flags,
		capability_flags_high
	);
	if (err != Error::ok) return err;

	// Compose capabilities
	auto capabilities_begin = reinterpret_cast<std::uint8_t*>(&output.capability_falgs.value);
	memcpy(capabilities_begin, capability_flags_low.value.data(), 2);
	memcpy(capabilities_begin + 2, capability_flags_high.value.data(), 2);
	boost::endian::little_to_native_inplace(output.capability_falgs.value);

	// Check minimum server capabilities to deserialize this frame
	capabilities cap (output.capability_falgs.value);
	if (!cap.has(CLIENT_PLUGIN_AUTH)) return Error::server_unsupported;

	// Deserialize the rest of the frame
	err = deserialize_fields(
		ctx,
		auth_plugin_data_len,
		reserved
	);
	if (err != Error::ok) return err;
	auto auth2_length = static_cast<std::uint8_t>(
		std::max(13, auth_plugin_data_len.value - auth1_length));
	err = ctx.copy(output.auth_plugin_data_buffer.data() + auth1_length, auth2_length);
	if (err != Error::ok) return err;
	err = deserialize(output.auth_plugin_name, ctx);
	if (err != Error::ok) return err;

	// Compose auth_plugin_data
	memcpy(
		output.auth_plugin_data_buffer.data(),
		auth_plugin_data_part_1.value.data(),
		auth1_length
	);
	output.auth_plugin_data.value = std::string_view(
		output.auth_plugin_data_buffer.data(),
		auth1_length + auth2_length - 1 // discard trailing null byte
	);

	return Error::ok;
}

std::size_t boost::mysql::detail::get_size(
	const handshake_response_packet& value,
	const SerializationContext& ctx
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

inline void boost::mysql::detail::serialize(
	const handshake_response_packet& value,
	SerializationContext& ctx
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

inline boost::mysql::Error boost::mysql::detail::deserialize(
	auth_switch_request_packet& output,
	DeserializationContext& ctx
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

inline boost::mysql::Error boost::mysql::detail::deserialize(
	column_definition_packet& output,
	DeserializationContext& ctx
) noexcept
{
	int_lenenc length_of_fixed_fields;
	int2 final_padding;
	return deserialize_fields(
		ctx,
		output.catalog,
		output.schema,
		output.table,
		output.org_table,
		output.name,
		output.org_name,
		length_of_fixed_fields,
		output.character_set,
		output.column_length,
		output.type,
		output.flags,
		output.decimals,
		final_padding
	);
}

inline boost::mysql::Error boost::mysql::detail::deserialize(
	com_stmt_prepare_ok_packet& output,
	DeserializationContext& ctx
) noexcept
{
	int1 reserved;
	return deserialize_fields(
		ctx,
		output.statement_id,
		output.num_columns,
		output.num_params,
		reserved,
		output.warning_count
	);
}

template <typename ForwardIterator>
inline std::size_t boost::mysql::detail::get_size(
	const com_stmt_execute_packet<ForwardIterator>& value,
	const SerializationContext& ctx
) noexcept
{
	std::size_t res = 1 + // command ID
		get_size(value.statement_id, ctx) +
		get_size(value.flags, ctx) +
		get_size(value.iteration_count, ctx);
	auto num_params = std::distance(value.params_begin, value.params_end);
	assert(num_params >= 0 && num_params <= 255);
	res += null_bitmap_traits(stmt_execute_null_bitmap_offset, num_params).byte_count();
	res += get_size(value.new_params_bind_flag, ctx);
	res += get_size(com_stmt_execute_param_meta_packet{}, ctx) * num_params;
	for (auto it = value.params_begin; it != value.params_end; ++it)
	{
		res += get_size(*it, ctx);
	}
	return res;
}

template <typename ForwardIterator>
inline void boost::mysql::detail::serialize(
	const com_stmt_execute_packet<ForwardIterator>& input,
	SerializationContext& ctx
) noexcept
{
	serialize(int1(com_stmt_execute_packet<ForwardIterator>::command_id), ctx);
	serialize(input.statement_id, ctx);
	serialize(input.flags, ctx);
	serialize(input.iteration_count, ctx);

	// Number of parameters
	auto num_params = std::distance(input.params_begin, input.params_end);
	assert(num_params >= 0 && num_params <= 255);

	// NULL bitmap (already size zero if num_params == 0)
	null_bitmap_traits traits (stmt_execute_null_bitmap_offset, num_params);
	std::size_t i = 0;
	std::memset(ctx.first(), 0, traits.byte_count()); // Initialize to zeroes
	for (auto it = input.params_begin; it != input.params_end; ++it, ++i)
	{
		if (std::holds_alternative<std::nullptr_t>(*it))
		{
			traits.set_null(ctx.first(), i);
		}
	}
	ctx.advance(traits.byte_count());

	// new parameters bind flag
	serialize(input.new_params_bind_flag, ctx);

	// value metadata
	com_stmt_execute_param_meta_packet meta;
	for (auto it = input.params_begin; it != input.params_end; ++it)
	{
		meta.type = get_protocol_field_type(*it);
		meta.unsigned_flag.value = is_unsigned(*it) ? 0x80 : 0;
		serialize(meta, ctx);
	}

	// actual values
	for (auto it = input.params_begin; it != input.params_end; ++it)
	{
		serialize(*it, ctx);
	}
}

template <typename Serializable, typename Allocator>
void boost::mysql::detail::serialize_message(
	const Serializable& input,
	capabilities caps,
	basic_bytestring<Allocator>& buffer
)
{
	SerializationContext ctx (caps);
	std::size_t size = get_size(input, ctx);
	buffer.resize(size);
	ctx.set_first(buffer.data());
	serialize(input, ctx);
	assert(ctx.first() == buffer.data() + buffer.size());
}

template <typename Deserializable>
boost::mysql::error_code boost::mysql::detail::deserialize_message(
	Deserializable& output,
	DeserializationContext& ctx
)
{
	auto err = deserialize(output, ctx);
	if (err != Error::ok) return make_error_code(err);
	if (!ctx.empty()) return make_error_code(Error::extra_bytes);
	return error_code();
}


inline std::pair<boost::mysql::error_code, std::uint8_t>
boost::mysql::detail::deserialize_message_type(
	DeserializationContext& ctx
)
{
	int1 msg_type;
	std::pair<mysql::error_code, std::uint8_t> res {};
	auto err = deserialize(msg_type, ctx);
	if (err == Error::ok)
	{
		res.second = msg_type.value;
	}
	else
	{
		res.first = make_error_code(err);
	}
	return res;
}

inline boost::mysql::error_code boost::mysql::detail::process_error_packet(
	DeserializationContext& ctx,
	error_info& info
)
{
	err_packet error_packet;
	auto code = deserialize_message(error_packet, ctx);
	if (code) return code;
	info.set_message(std::string(error_packet.error_message.value));
	return make_error_code(static_cast<Error>(error_packet.error_code.value));
}


#endif
