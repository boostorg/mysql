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

void mysql::AnyPacketReader::read(boost::asio::ip::tcp::socket& stream)
{
	// Connection phase
	uint8_t header_buffer [4];
	boost::asio::read(stream, boost::asio::buffer(header_buffer));
	deserialize(header_buffer, std::end(header_buffer), header_);

	// Read the rest of the packet
	buffer_.reset(new uint8_t[header_.packet_size.value]);
	boost::asio::read(stream, boost::asio::mutable_buffer{buffer_.get(), header_.packet_size.value});

	deserialize(buffer_.get(), end(), message_type_);
}

void mysql::AnyPacketReader::check_error() const
{
	if (is_error())
	{
		ErrPacket error_packet;
		deserialize_message(error_packet);
		std::ostringstream ss;
		ss << "SQL error: " << error_packet.error_message.value
		   << " (" << error_packet.error_code << ")";
		throw std::runtime_error { ss.str() };
	}
}

mysql::AnyPacketWriter::AnyPacketWriter(int1 sequence_number)
{
	std::uint8_t initial_content [] = {0, 0, 0, sequence_number};
	buffer_.add(initial_content, sizeof(initial_content));
}

void mysql::AnyPacketWriter::set_length()
{
	assert(buffer_.size() <= 0xffffff); // TODO: handle the case where this does not hold
	int3 packet_length { static_cast<std::uint32_t>(buffer_.size() - 4) };
	boost::endian::native_to_little_inplace(packet_length.value);
	memcpy(buffer_.data(), &packet_length.value, 3);
}

void mysql::AnyPacketWriter::write(boost::asio::ip::tcp::socket& stream)
{
	boost::asio::write(stream, boost::asio::buffer(buffer_.data(), buffer_.size()));
}

void mysql::MysqlStream::process_sequence_number(int1 got)
{
	if (got != sequence_number_++)
		throw std::runtime_error { "Mismatched sequence number" };
}

void mysql::MysqlStream::read_packet(AnyPacketReader& packet)
{
	packet.read(next_layer_);
	process_sequence_number(packet.header().sequence_number);
}

void mysql::MysqlStream::handshake(const HandshakeParams& params)
{
	AnyPacketReader reader;
	read_packet(reader);

	if (reader.message_type() != handshake_protocol_version_10)
	{
		const char* reason = reader.message_type() == handshake_protocol_version_9 ?
				"Unsupported protocol version 9" :
				"Unknown message type";
		throw std::runtime_error {reason};
	}

	// Process the handshake
	mysql::Handshake handshake;
	reader.deserialize_message(handshake);
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
	AnyPacketWriter writer { sequence_number_++ };
	writer.serialize_message(handshake_response);
	writer.write(next_layer_);

	// TODO: support auth mismatch
	// TODO: support SSL

	// Read the OK/ERR
	read_packet(reader);
	if (!reader.is_ok() && !reader.is_eof())
	{
		throw std::runtime_error { "Unknown message type" };
	}
	std::cout << "Connected to server\n";
}


