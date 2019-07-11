/*
 * mysql_stream.cpp
 *
 *  Created on: Jul 7, 2019
 *      Author: ruben
 */

#include "mysql_stream.hpp"
#include "message_serialization.hpp"
#include "auth.hpp"
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <iostream>
#include <cassert>
#include <sstream>

using namespace std;

template <typename T1, typename... Flags>
constexpr bool all_set(T1 input, Flags... flags)
{
	return ((input & flags) && ...);
}

static void check_capabilities(mysql::int4 server_capabilities)
{
	bool ok = all_set(server_capabilities,
		mysql::CLIENT_CONNECT_WITH_DB,
		mysql::CLIENT_PROTOCOL_41,
		mysql::CLIENT_PLUGIN_AUTH,
		mysql::CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA,
		mysql::CLIENT_DEPRECATE_EOF
	);
	if (!ok)
		throw std::runtime_error { "Missing server capabilities, server not supported" };
}

static void check_authentication_method(const mysql::Handshake& handshake)
{
	if (handshake.auth_plugin_name.value != "mysql_native_password")
		throw std::runtime_error { "Unsupported authentication method" }; // TODO: we should be responding with our method
	if (handshake.auth_plugin_data.size() != mysql::mysql_native_password::challenge_length)
		throw std::runtime_error { "Bad authentication data length" };
}

constexpr mysql::int4 BASIC_CAPABILITIES_FLAGS =
		mysql::CLIENT_PROTOCOL_41 |
		mysql::CLIENT_PLUGIN_AUTH |
		mysql::CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA |
		mysql::CLIENT_DEPRECATE_EOF |
		mysql::CLIENT_CONNECT_WITH_DB;

mysql::int1 mysql::get_message_type(const std::vector<std::uint8_t>& buffer, bool check_err)
{
	mysql::int1 res;
	ReadIterator current = mysql::deserialize(buffer, res);
	if (check_err && res == error_packet_header)
	{
		ErrPacket error_packet;
		deserialize(current, buffer.data() + buffer.size(), error_packet);
		std::ostringstream ss;
		ss << "SQL error: " << error_packet.error_message.value
		   << " (" << error_packet.error_code << ")";
		throw std::runtime_error { ss.str() };
	}
	return res;
}


void mysql::MysqlStream::process_sequence_number(int1 got)
{
	if (got != sequence_number_++)
		throw std::runtime_error { "Mismatched sequence number" };
}

void mysql::MysqlStream::read(std::vector<std::uint8_t>& buffer)
{
	PacketHeader header;
	uint8_t header_buffer [4];
	std::size_t current_size = 0;
	do
	{
		boost::asio::read(next_layer_, boost::asio::buffer(header_buffer));
		deserialize(std::begin(header_buffer), std::end(header_buffer), header);
		process_sequence_number(header.sequence_number);
		buffer.resize(current_size + header.packet_size.value);
		boost::asio::read(next_layer_, boost::asio::buffer(buffer.data() + current_size, header.packet_size.value));
		current_size += header.packet_size.value;
	} while (header.packet_size.value == 0xffffff);
}

void mysql::MysqlStream::write(const std::vector<std::uint8_t>& buffer)
{
	PacketHeader header;
	DynamicBuffer header_buffer; // TODO: change to a plain uint8_t when we generalize this
	std::size_t current_size = 0;
	while (current_size < buffer.size())
	{
		auto size_to_write = static_cast<std::uint32_t>(std::min(
				std::vector<std::uint8_t>::size_type(0xffffff),
				buffer.size() - current_size
		));
		header.packet_size.value = size_to_write;
		header.sequence_number = sequence_number_++;
		serialize(header_buffer, header);
		boost::asio::write(next_layer_, boost::asio::buffer(header_buffer.data(), header_buffer.size()));
		boost::asio::write(next_layer_, boost::asio::buffer(buffer.data() + current_size, size_to_write));
		current_size += size_to_write;
	}
}

void mysql::MysqlStream::handshake(const HandshakeParams& params)
{
	std::vector<std::uint8_t> read_buffer;
	DynamicBuffer write_buffer;

	// Read handshake
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
	check_capabilities(handshake.capability_falgs);
	check_authentication_method(handshake);
	cout << handshake << "\n\n";

	// Response
	mysql::HandshakeResponse handshake_response;
	mysql_native_password::response_buffer auth_response;
	mysql_native_password::compute_auth_string(params.password, handshake.auth_plugin_data.data(), auth_response);
	handshake_response.client_flag = BASIC_CAPABILITIES_FLAGS;
	handshake_response.max_packet_size = 0xffff;
	handshake_response.character_set = params.character_set;
	handshake_response.username.value = params.username;
	handshake_response.auth_response.value = string_view {(const char*)auth_response, sizeof(auth_response)};
	handshake_response.client_plugin_name.value = "mysql_native_password";
	handshake_response.database.value = params.database;
	cout << handshake_response << "\n\n";

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


