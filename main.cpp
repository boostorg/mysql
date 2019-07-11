#include <iostream>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include "mysql_stream.hpp"
#include "prepared_statement.hpp"

using namespace std;
using namespace boost::asio;
using namespace mysql;

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

	// MYSQL stream
	MysqlStream stream {ctx};

	// TCP connection
	stream.next_layer().connect(endpoint);

	// Handshake
	stream.handshake(mysql::HandshakeParams{
		CharacterSetLowerByte::utf8_general_ci,
		"root",
		"root",
		"mysql"
	});

	// Prepare a statement

	mysql::PreparedStatement stmt { mysql::PreparedStatement::prepare(
			stream, "SELECT host FROM user WHERE user = ?") };


}
