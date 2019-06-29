#include <iostream>
#include <cstdint>
#include <boost/endian/conversion.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include "basic_types.hpp"
#include "deserialization.hpp"

using namespace std;
using namespace boost::asio;

constexpr auto HOSTNAME = "localhost"sv;
constexpr auto PORT = "3306"sv;

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
	unsigned char char_buffer [1024];
	mutable_buffer buffer {char_buffer, sizeof(char_buffer)};

	// Connection phase

	// Read packet header
	boost::asio::read(sock, mutable_buffer{char_buffer, 4});
	mysql::int3 packet_size;
	deserialize(char_buffer, char_buffer+4, packet_size);
	uint8_t sequence_number = char_buffer[3];

	// Read the rest of the packet
	std::unique_ptr<unsigned char[]> packet_buffer { new unsigned char[packet_size.value] };
	boost::asio::read(sock, mutable_buffer{packet_buffer.get(), packet_size.value});
	// TODO: handling the case where the packet is bigger than X bytes

	//   Either ERR
	//   Or Handshake



	post(ctx, [&guard] { cout << "Hello word\n"; guard.reset(); });
	ctx.run();
}
