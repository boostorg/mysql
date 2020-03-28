
#include "boost/mysql/connection.hpp"
#include <boost/asio/io_service.hpp>
#include <boost/system/system_error.hpp>
#include <boost/asio/spawn.hpp>
#include <iostream>

using boost::mysql::error_code;
using boost::mysql::error_info;

/**
 * For this example, we will be using the 'mysql_asio_examples' database.
 * You can get this database by running db_setup.sql.
 * This example assumes you are connecting to a localhost MySQL server.
 *
 * This example uses asynchronous functions with coroutines.
 *
 * This example assumes you are already familiar with the basic concepts
 * of mysql-asio (tcp_connection, resultset, rows, values). If you are not,
 * please have a look to the query_sync.cpp example.
 */

void print_employee(const boost::mysql::row& employee)
{
	using boost::mysql::operator<<; // Required for mysql::value objects to be streamable, due to ADL rules
	std::cout << "Employee '"
			  << employee.values()[0] << " "                   // first_name (type std::string_view)
			  << employee.values()[1] << "' earns "            // last_name  (type std::string_view)
			  << employee.values()[2] << " dollars yearly\n";  // salary     (type double)
}

void check_error(
	const error_code& err,
	const boost::mysql::error_info& info = boost::mysql::error_info()
)
{
	if (err)
	{
		throw boost::system::system_error(err, info.message());
	}
}


void main_impl(int argc, char** argv)
{
	if (argc != 3)
	{
		std::cerr << "Usage: " << argv[0] << " <username> <password>\n";
		exit(1);
	}

	// Context and connections
	boost::asio::io_context ctx;
	boost::mysql::tcp_connection conn (ctx);

	boost::asio::ip::tcp::endpoint ep (
		boost::asio::ip::address_v4::loopback(), // host
		boost::mysql::default_port			     // port
	);
	boost::mysql::connection_params params (
		argv[1],               // username
		argv[2],		       // password
		"mysql_asio_examples"  // database to use; leave empty or omit the parameter for no database
	);

	// Entry point
	boost::asio::spawn(ctx.get_executor(), [&conn, ep, params](boost::asio::yield_context yield) {
		boost::mysql::error_code ec;

		// TCP connect
		conn.next_layer().async_connect(ep, yield[ec]);
		check_error(ec);

		// MySQL handshake
		boost::mysql::error_info errinfo = conn.async_handshake(params, yield[ec]);
		check_error(ec, errinfo);

		// Issue the query to the server
		const char* sql = "SELECT first_name, last_name, salary FROM employee WHERE company_id = 'HGS'";
		boost::mysql::async_handler_arg<boost::mysql::tcp_resultset> result =
				conn.async_query(sql, yield[ec]);
		check_error(ec, result.error());

		// Get all the rows in the resultset
		while (true)
		{
			boost::mysql::async_handler_arg<const boost::mysql::row*> row =
					result.get().async_fetch_one(yield[ec]);
			check_error(ec, row.error());
			if (!row.get()) break;
			print_employee(*row.get());
		}
	});

	ctx.run();
}

int main(int argc, char** argv)
{
	try
	{
		main_impl(argc, argv);
	}
	catch (const boost::system::system_error& err)
	{
		std::cerr << "Error: " << err.what() << ", error code: " << err.code() << std::endl;
		return 1;
	}
	catch (const std::exception& err)
	{
		std::cerr << "Error: " << err.what() << std::endl;
		return 1;
	}
}
