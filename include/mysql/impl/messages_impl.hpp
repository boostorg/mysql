#ifndef INCLUDE_MYSQL_IMPL_MESSAGES_IMPL_HPP_
#define INCLUDE_MYSQL_IMPL_MESSAGES_IMPL_HPP_

#include "mysql/impl/basic_serialization.hpp"
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
	std::string_view auth_plugin_data_part_2;

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


#endif /* INCLUDE_MYSQL_IMPL_MESSAGES_IMPL_HPP_ */
