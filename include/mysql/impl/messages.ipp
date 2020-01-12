#ifndef MYSQL_ASIO_IMPL_MESSAGES_IPP
#define MYSQL_ASIO_IMPL_MESSAGES_IPP

#include "mysql/impl/serialization.hpp"
#include <cassert>

inline mysql::Error mysql::detail::deserialize(
	msgs::ok_packet& output,
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

inline mysql::Error mysql::detail::deserialize(
	msgs::handshake& output,
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
	auto auth2_length = std::max(13, auth_plugin_data_len.value - auth1_length);
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

std::size_t mysql::detail::get_size(
	const msgs::handshake_response& value,
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

inline void mysql::detail::serialize(
	const msgs::handshake_response& value,
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

inline mysql::Error mysql::detail::deserialize(
	msgs::auth_switch_request& output,
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

inline mysql::Error mysql::detail::deserialize(
	msgs::column_definition& output,
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

template <typename Serializable, typename Allocator>
void mysql::detail::serialize_message(
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
mysql::error_code mysql::detail::deserialize_message(
	Deserializable& output,
	DeserializationContext& ctx
)
{
	auto err = deserialize(output, ctx);
	if (err != Error::ok) return make_error_code(err);
	if (!ctx.empty()) return make_error_code(Error::extra_bytes);
	return error_code();
}


inline mysql::error_code mysql::detail::deserialize_message_type(
	std::uint8_t& output,
	DeserializationContext& ctx
)
{
	int1 msg_type;
	auto err = deserialize(msg_type, ctx);
	if (err != Error::ok) return make_error_code(err);
	output = msg_type.value;
	return error_code();
}

inline mysql::error_code mysql::detail::process_error_packet(
	DeserializationContext& ctx
)
{
	msgs::err_packet error_packet;
	auto errc = deserialize_message(error_packet, ctx);
	if (errc) return errc;
	return make_error_code(static_cast<Error>(error_packet.error_code.value));
}


#endif
