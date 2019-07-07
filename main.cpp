#include <iostream>
#include <cstdint>
#include <bitset>
#include <boost/endian/conversion.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <cassert>
#include <stdexcept>
#include "basic_types.hpp"
#include "auth.hpp"
#include "message_serialization.hpp"

using namespace std;
using namespace boost::asio;
using namespace mysql;

constexpr auto HOSTNAME = "localhost"sv;
constexpr auto PORT = "3306"sv;

struct RawPacket
{
	PacketHeader header;
	std::unique_ptr<std::uint8_t[]> buffer;

	ReadIterator begin() const { return buffer.get(); }
	ReadIterator end() const { return buffer.get() + header.packet_size.value; }
	ReadIterator body_begin() const { return begin() + 1; } // past message type, if any
	std::uint8_t message_type() const { std::uint8_t res; mysql::deserialize(begin(), end(), res); return res; }

	template <typename PacketType>
	void deserialize(PacketType& output)
	{
		ReadIterator last = mysql::deserialize(body_begin(), end(), output);
		if (last != end())
			throw std::out_of_range { "Additional data after packet end" };
	}
};

void read_packet(ip::tcp::socket& sock, RawPacket& output)
{
	// Connection phase
	uint8_t header_buffer [4];
	boost::asio::read(sock, boost::asio::buffer(header_buffer));
	deserialize(header_buffer, end(header_buffer), output.header);

	// Read the rest of the packet
	output.buffer.reset(new uint8_t[output.header.packet_size.value]);
	boost::asio::read(sock, mutable_buffer{output.buffer.get(), output.header.packet_size.value});
	// TODO: handling the case where the packet is bigger than X bytes
}

int main()
{
	// Basic
	io_context ctx;
	auto guard = make_work_guard(ctx);
	boost::system::error_code errc;

	// DNS resolution
	ip::tcp::resolver resolver {ctx};
	auto results = resolver.resolve(ip::tcp::v4(), HOSTNAME, PORT);
	if (results.size() != 1)
	{
		cout << "Found endpoints: " << results.size() << ", exiting" << endl;
		exit(1);
	}
	auto endpoint = results.begin()->endpoint();
	cout << "Connecting to: " << endpoint << endl;

	// TCP connection
	ip::tcp::socket sock {ctx};
	sock.connect(endpoint);

	// Connection phase
	RawPacket packet;
	read_packet(sock, packet);
	uint8_t msg_type = packet.message_type();

	if (msg_type == handshake_protocol_version_10) // handshake
	{
		mysql::Handshake handshake;
		packet.deserialize(handshake);
		std::cout <<
				"server_version=" << handshake.server_version.value << ",\n" <<
				"connection_id="  << handshake.connection_id  << ",\n" <<
				"auth_plugin_data=" << handshake.auth_plugin_data << ",\n" <<
				"capability_falgs=" << std::bitset<32>{handshake.capability_falgs} << ",\n" <<
				"character_set=" << static_cast<int1>(handshake.character_set) << ",\n" <<
				"status_flags=" << std::bitset<16>{handshake.status_flags} << ",\n" <<
				"auth_plugin_name=" << handshake.auth_plugin_name.value << "\n\n";
		mysql::HandshakeResponse handshake_response;
		assert(handshake.auth_plugin_data.size() == mysql_native_password::challenge_length);
		mysql_native_password::response_buffer auth_response;
		mysql_native_password::compute_auth_string("root", handshake.auth_plugin_data.data(), auth_response);
		handshake_response.client_flag =
				CLIENT_CONNECT_WITH_DB |
				CLIENT_PROTOCOL_41 |
				CLIENT_PLUGIN_AUTH |
				CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA |
				CLIENT_DEPRECATE_EOF;
		handshake_response.max_packet_size = 0xffff;
		handshake_response.character_set = CharacterSetLowerByte::utf8_general_ci;
		handshake_response.username.value = "root";
		handshake_response.auth_response.value = string_view {(const char*)auth_response, sizeof(auth_response)};
		handshake_response.database.value = "mysql";
		handshake_response.client_plugin_name.value = "mysql_native_password";

		// Serialize and send
		DynamicBuffer response_buffer { ++packet.header.sequence_number };
		serialize(response_buffer, handshake_response);
		response_buffer.set_size();
		boost::asio::write(sock, boost::asio::buffer(response_buffer.data(), response_buffer.size()));

		// Read the OK/ERR
		read_packet(sock, packet);
		msg_type = packet.message_type();
		if (msg_type == ok_packet_header || msg_type == eof_packet_header)
		{
			cout << "Successfully connected to server\n";
			// TODO
		}
		else if (msg_type == error_packet_header)
		{
			ErrPacket error_packet;
			packet.deserialize(error_packet);
			std::cerr << "Handshake resulted in error: " << error_packet.error_code
					  << ": " << error_packet.error_message.value << endl;
		}
		else
		{
			cout << "Message type is: " << hex << msg_type << endl;
		}


	}
	else
	{
		cout << "Message type is: " << hex << msg_type << endl;
	}

	//   Either ERR
	//   Or Handshake



	//post(ctx, [&guard] { cout << "Hello word\n"; guard.reset(); });
	//ctx.run();
}
