#ifndef INCLUDE_MYSQL_IMPL_HANDSHAKE_IMPL_HPP_
#define INCLUDE_MYSQL_IMPL_HANDSHAKE_IMPL_HPP_

#include "mysql/impl/basic_serialization.hpp"
#include "mysql/error.hpp"

/*
* CLIENT_LONG_PASSWORD: unset //  Use the improved version of Old Password Authentication
* CLIENT_FOUND_ROWS: unset //  Send found rows instead of affected rows in EOF_Packet
* CLIENT_LONG_FLAG: unset //  Get all column flags
* CLIENT_CONNECT_WITH_DB: optional //  Database (schema) name can be specified on connect in Handshake Response Packet
* CLIENT_NO_SCHEMA: unset //  Don't allow database.table.column
* CLIENT_COMPRESS: unset //  Compression protocol supported
* CLIENT_ODBC: unset //  Special handling of ODBC behavior
* CLIENT_LOCAL_FILES: unset //  Can use LOAD DATA LOCAL
* CLIENT_IGNORE_SPACE: unset //  Ignore spaces before '('
* CLIENT_PROTOCOL_41: mandatory //  New 4.1 protocol
* CLIENT_INTERACTIVE: unset //  This is an interactive client
* CLIENT_SSL: unset //  Use SSL encryption for the session
* CLIENT_IGNORE_SIGPIPE: unset //  Client only flag
* CLIENT_TRANSACTIONS: unset //  Client knows about transactions
* CLIENT_RESERVED: unset //  DEPRECATED: Old flag for 4.1 protocol
* CLIENT_RESERVED2: unset //  DEPRECATED: Old flag for 4.1 authentication \ CLIENT_SECURE_CONNECTION
* CLIENT_MULTI_STATEMENTS: unset //  Enable/disable multi-stmt support
* CLIENT_MULTI_RESULTS: unset //  Enable/disable multi-results
* CLIENT_PS_MULTI_RESULTS: unset //  Multi-results and OUT parameters in PS-protocol
* CLIENT_PLUGIN_AUTH: mandatory //  Client supports plugin authentication
* CLIENT_CONNECT_ATTRS: unset //  Client supports connection attributes
* CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA: mandatory //  Enable authentication response packet to be larger than 255 bytes
* CLIENT_CAN_HANDLE_EXPIRED_PASSWORDS: unset //  Don't close the connection for a user account with expired password
* CLIENT_SESSION_TRACK: unset //  Capable of handling server state change information
* CLIENT_DEPRECATE_EOF: mandatory //  Client no longer needs EOF_Packet and will use OK_Packet instead
* CLIENT_SSL_VERIFY_SERVER_CERT: unset //  Verify server certificate
* CLIENT_OPTIONAL_RESULTSET_METADATA: unset //  The client can handle optional metadata information in the resultset
* CLIENT_REMEMBER_OPTIONS: unset //  Don't reset the options after an unsuccessful connect
*
* We pay attention to:
* CLIENT_CONNECT_WITH_DB: optional //  Database (schema) name can be specified on connect in Handshake Response Packet
* CLIENT_PROTOCOL_41: mandatory //  New 4.1 protocol
* CLIENT_PLUGIN_AUTH: mandatory //  Client supports plugin authentication
* CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA: mandatory //  Enable authentication response packet to be larger than 255 bytes
* CLIENT_DEPRECATE_EOF: mandatory //  Client no longer needs EOF_Packet and will use OK_Packet instead
 */

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
		err = make_error_code(Error::server_unsupported);
		return;
	}
	else if (msg_type.value != handshake_protocol_version_10)
	{
		err = make_error_code(Error::protocol_value_error);
		return;
	}
	if (!ctx.empty())
	{
		err = make_error_code(Error::extra_bytes);
		return;
	}


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
