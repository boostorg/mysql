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

struct VariantPrinter
{
	template <typename T>
	void operator()(T v) const { cout << v << ", "; }

	void operator()(string_lenenc v) const { (*this)(v.value); }
	void operator()(std::nullptr_t) const { (*this)("NULL"); }
};

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
		"awesome"
	});

	// Prepare a statement

	/*mysql::PreparedStatement stmt { mysql::PreparedStatement::prepare(
			stream, "SELECT first_name, age FROM users WHERE last_name = ?") };
	stmt.execute(string_lenenc {"user"});*/


	mysql::PreparedStatement stmt { mysql::PreparedStatement::prepare(
			stream, "SELECT * from users WHERE age < ? and first_name <> ?") };
	auto res = stmt.execute(40, string_lenenc{"hola"});
	for (const auto& row: res)
	{
		for (const auto& field: row.values())
		{
			std::visit(VariantPrinter(), field);
		}
		std::cout << "\n";
	}

}
