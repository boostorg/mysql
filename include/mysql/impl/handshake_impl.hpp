#ifndef INCLUDE_MYSQL_IMPL_HANDSHAKE_IMPL_HPP_
#define INCLUDE_MYSQL_IMPL_HANDSHAKE_IMPL_HPP_

#include "mysql/impl/basic_serialization.hpp"
#include "mysql/impl/capabilities.hpp"
#include "mysql/impl/auth.hpp"
#include "mysql/error.hpp"

// TODO: support compression, SSL, more authentication methods
template <typename ChannelType, typename Allocator>
void mysql::detail::hanshake(
	ChannelType& channel,
	const handshake_params& params,
	std::vector<std::uint8_t, Allocator>& buffer,
	capabilities& output_capabilities,
	error_code& err
)
{
	buffer.clear();

	// Read server greeting
	channel.read(buffer, err);
	if (err) return;

	// Deserialize server greeting
	msgs::handshake handshake;
	DeserializationContext ctx (buffer.data(), buffer.data() + buffer.size(), capabilities(0));
	int1 msg_type;
	err = make_error_code(deserialize(msg_type, ctx));
	if (err) return;
	if (msg_type.value == error_packet_header)
	{
		err = make_error_code(Error::server_returned_error);
		return;
	}
	else if (msg_type.value == handshake_protocol_version_9)
	{
		err = make_error_code(Error::server_unsupported);
		return;
	}
	else if (msg_type.value != handshake_protocol_version_10)
	{
		err = make_error_code(Error::protocol_value_error);
		return;
	}
	err = make_error_code(deserialize(handshake, ctx));
	if (err) return;
	if (!ctx.empty())
	{
		err = make_error_code(Error::extra_bytes);
		return;
	}

	// Check capabilities
	capabilities server_caps (handshake.capability_falgs.value);
	capabilities required_caps =
			params.database.empty() ?
			mandatory_capabilities :
			mandatory_capabilities | capabilities(CLIENT_CONNECT_WITH_DB);
	if (!server_caps.has_all(required_caps))
	{
		err = make_error_code(Error::server_unsupported);
		return;
	}
	auto final_caps = server_caps & (required_caps | optional_capabilities);
	output_capabilities = final_caps;

	// Authentication
	std::array<std::uint8_t, mysql_native_password::response_length> auth_response_buffer {};
	std::string_view auth_response;
	if (handshake.auth_plugin_name.value == mysql_native_password::plugin_name)
	{
		if (handshake.auth_plugin_data.value.size() != mysql_native_password::challenge_length)
		{
			err = make_error_code(Error::protocol_value_error);
			return;
		}
		mysql_native_password::compute_auth_string(
			params.password,
			handshake.auth_plugin_data.value.data(),
			auth_response_buffer.data()
		);
		auth_response = std::string_view(reinterpret_cast<const char*>(auth_response.data()), auth_response.size());
	}

	// Compose response
	msgs::handshake_response response;
	response.client_flag.value = final_caps.get();
	response.max_packet_size.value = MAX_PACKET_SIZE;
	response.character_set = params.character_set;
	response.username.value = params.username;
	response.auth_response.value = auth_response;
	response.database.value = params.database;
	response.client_plugin_name.value = mysql_native_password::plugin_name;

	// Serialize
	serialize_message(response, final_caps, buffer);

	// Send
	channel.write(boost::asio::buffer(buffer), err);
	if (err) return;

	// Receive response
	buffer.clear();
	channel.read(buffer, err);
	if (err) return;

	// Deserialize it
	ctx = DeserializationContext(buffer.data(), buffer.data() + buffer.size(), final_caps);
	err = make_error_code(deserialize(msg_type, ctx));
	if (err) return;
	if (msg_type.value == ok_packet_header)
	{
		// Auth success via fast auth path
		// TODO: is this OK_Packet useful for anything?
		err.clear();
		return;
	}
	else if (msg_type.value == error_packet_header)
	{
		// TODO: provide more information here
		err = make_error_code(Error::auth_error);
		return;
	}
	else if (msg_type.value != auth_switch_request_header)
	{
		err = make_error_code(Error::protocol_value_error);
		return;
	}

	// We have received an auth switch request. Deserialize it
	msgs::auth_switch_request auth_sw;
	err = make_error_code(deserialize(auth_sw, ctx));
	if (err) return;
	if (!ctx.empty())
	{
		err = make_error_code(Error::extra_bytes);
		return;
	}

	// Compute response
	msgs::auth_switch_response auth_sw_res;
	if (auth_sw.plugin_name.value != mysql_native_password::plugin_name)
	{
		err = make_error_code(Error::unknown_auth_plugin);
		return;
	}
	if (auth_sw.auth_plugin_data.value.size() != mysql_native_password::challenge_length)
	{
		err = make_error_code(Error::protocol_value_error);
		return;
	}
	mysql_native_password::compute_auth_string(
		params.password,
		handshake.auth_plugin_data.value.data(),
		auth_response_buffer.data()
	);
	auth_sw_res.auth_plugin_data.value = std::string_view(
		reinterpret_cast<const char*>(auth_response_buffer.data()),
		auth_response_buffer.size()
	);

	// Serialize
	serialize_message(auth_sw_res, final_caps, buffer);

	// Send
	channel.write(boost::asio::buffer(buffer), err);
	if (err) return;

	// Receive response
	channel.read(buffer, err);
	if (err) return;
	ctx = DeserializationContext(buffer.data(), buffer.data() + buffer.size(), final_caps);
	err = make_error_code(deserialize(msg_type, ctx));
	if (err) return;
	if (msg_type.value == ok_packet_header)
	{
		err.clear();
	}
	else if (msg_type.value == error_packet_header)
	{
		err = make_error_code(Error::auth_error);
	}
	else
	{
		err = make_error_code(Error::protocol_value_error);
	}
}



#endif /* INCLUDE_MYSQL_IMPL_HANDSHAKE_IMPL_HPP_ */
