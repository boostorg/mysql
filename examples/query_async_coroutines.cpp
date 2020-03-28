
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

// Throws an exception if an operation failed
void check_error(
	const error_code& err,
	const error_info& info = {}
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

	/**
	 * The entry point. We spawn a stackful coroutine using boost::asio::spawn
	 * (see https://www.boost.org/doc/libs/1_72_0/doc/html/boost_asio/reference/spawn.html).
	 *
	 * The coroutine will actually start running when we call io_context::run().
	 * It will suspend every time we call one of the asyncrhonous functions, saving
	 * all information it needs for resuming. When the asynchronous operation completes,
	 * the coroutine will resume in the point it was left.
	 *
	 * The return type of a coroutine is the second argument to the handler signature
	 * for the asynchronous operation. For example, connection::connect has a handler
	 * signature of void(error_code, error_info), so the coroutine return type is error_info.
	 *
	 * Coroutines are limited to returning a single argument, so all handler signatures
	 * in boost::mysql are limited to two arguments. In boost::mysql, coroutines may return:
	 *   - error_info. Provides additional information in case of error.
	 *   - async_handler_arg<T>. A combination of a value of type T and an error_info.
	 *     Used by functions like connection::async_query(), which has to transmit
	 *     a resultset as a return value, in addition to the error_info.
	 */
	boost::asio::spawn(ctx.get_executor(), [&conn, ep, params](boost::asio::yield_context yield) {
		// This error_code will be filled if an operation fails. We will check it
		// for every operation we perform.
		boost::mysql::error_code ec;

		// TCP connect
		conn.next_layer().async_connect(ep, yield[ec]);
		check_error(ec);

		// MySQL handshake. Note that if the operation would fail,
		// the returned error_info would contain additional information about what happened
		boost::mysql::error_info errinfo = conn.async_handshake(params, yield[ec]);
		check_error(ec, errinfo);

		// Issue the query to the server. This returns an async_handler_arg<tcp_resultset>,
		// which contains an error_info and a tcp_resultset. Call async_handler_arg::error()
		// to obtain the error_info, which will contain additional info in case of error.
		// async_handler_arg::get() returns the actual resultset.
		const char* sql = "SELECT first_name, last_name, salary FROM employee WHERE company_id = 'HGS'";
		boost::mysql::async_handler_arg<boost::mysql::tcp_resultset> result =
				conn.async_query(sql, yield[ec]);
		check_error(ec, result.error()); // The error_info

		/**
		 * Get all rows in the resultset. We will employ resultset::async_fetch_one(),
		 * which returns a single row at every call. The returned row is a pointer
		 * to memory owned by the resultset, and is re-used for each row. Thus, returned
		 * rows remain valid until the next call to async_fetch_one(). When no more
		 * rows are available, async_fetch_one returns nullptr.
		 */
		while (true)
		{
			boost::mysql::async_handler_arg<const boost::mysql::row*> row =
					result.get().async_fetch_one(yield[ec]);
			check_error(ec, row.error());
			if (!row.get()) break; // No more rows available
			print_employee(*row.get());
		}
	});

	// Don't forget to call run()! Otherwise, your program
	// will not spawn the coroutine and will do nothing.
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
