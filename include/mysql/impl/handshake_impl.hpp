#ifndef INCLUDE_MYSQL_IMPL_HANDSHAKE_IMPL_HPP_
#define INCLUDE_MYSQL_IMPL_HANDSHAKE_IMPL_HPP_

#include "mysql/impl/basic_serialization.hpp"
#include "mysql/error.hpp"

template <typename ChannelType, typename DynamicBuffer>
void mysql::detail::hanshake(
	ChannelType& channel,
	const handshake_params& params,
	DynamicBuffer& buffer,
	error_code& err
)
{
	buffer.shrink(buffer.size()); // clear

	// Read server greeting
	channel.read(buffer, err);
	if (err) return;

	// Deserialize server greeting
	boost::asio::const_buffer buffer_view = buffer.data(); // TODO: generalize this (really required?)
	auto first = static_cast<ReadIterator>(buffer_view.data());
	DeserializationContext ctx (first, first + buffer_view.size(), 0);
	int1 msg_type;
	err = deserialize(msg_type, ctx);
	if (err) return;
	if (msg_type.value == error_packet_header)
	{
		err = make_error_code(Error::server_returned_error);
		return;
	}
	else if (msg_type.value == handshake_protocol_version_9)
	{
		err = make_error_code(Error::unsupported_protocol_v9);
		return;
	}
	else if (msg_type.value != handshake_protocol_version_10)
	{
		err = make_error_code(Error::protocol_value_error);
		return;
	}




	read(read_buffer);
	auto msg_type = get_message_type(read_buffer);
	if (msg_type != handshake_protocol_version_10)
	{
		const char* reason = msg_type == handshake_protocol_version_9 ?
				"Unsupported protocol version 9" :
				"Unknown message type";
		throw std::runtime_error {reason};
	}
	mysql::Handshake handshake;
	deserialize(read_buffer.data()+1, read_buffer.data() + read_buffer.size(), handshake);

	// Process the handshake
	detail::check_capabilities(handshake.capability_falgs);
	detail::check_authentication_method(handshake);
	std::cout << handshake << "\n\n";

	// Response
	mysql::HandshakeResponse handshake_response;
	mysql_native_password::response_buffer auth_response;
	mysql_native_password::compute_auth_string(params.password, handshake.auth_plugin_data.data(), auth_response);
	handshake_response.client_flag = detail::BASIC_CAPABILITIES_FLAGS;
	handshake_response.max_packet_size = 0xffff;
	handshake_response.character_set = params.character_set;
	handshake_response.username.value = params.username;
	handshake_response.auth_response.value = std::string_view {(const char*)auth_response, sizeof(auth_response)};
	handshake_response.client_plugin_name.value = "mysql_native_password";
	handshake_response.database.value = params.database;
	std::cout << handshake_response << "\n\n";

	// Serialize and send
	serialize(write_buffer, handshake_response);
	write(write_buffer.get());

	// TODO: support auth mismatch
	// TODO: support SSL

	// Read the OK/ERR
	read(read_buffer);
	msg_type = get_message_type(read_buffer);
	if (msg_type != ok_packet_header && msg_type != eof_packet_header)
	{
		throw std::runtime_error { "Unknown message type" };
	}
	std::cout << "Connected to server\n";
}



#endif /* INCLUDE_MYSQL_IMPL_HANDSHAKE_IMPL_HPP_ */
