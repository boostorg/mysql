
#include "boost/mysql/connection.hpp"
#include <boost/asio/io_service.hpp>
#include <boost/system/system_error.hpp>
#include <boost/asio/use_future.hpp>
#include <iostream>
#include <thread>

using boost::mysql::error_code;
using boost::mysql::error_info;
using boost::asio::use_future;

/**
 * For this example, we will be using the 'mysql_asio_examples' database.
 * You can get this database by running db_setup.sql.
 * This example assumes you are connecting to a localhost MySQL server.
 *
 * This example uses asynchronous functions with futures.
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

/**
 * A boost::asio::io_context plus a thread that calls context.run().
 * We encapsulate this here to ensure correct shutdown even in case of
 * error (exception), when we should first reset the work guard, to
 * stop the io_context, and then join the thread. Failing to do so
 * may cause your application to not stop (if the work guard is not
 * reset) or to terminate badly (if the thread is not joined).
 */
class application
{
	boost::asio::io_context ctx_;
	boost::asio::executor_work_guard<boost::asio::io_context::executor_type> guard_;
	std::thread runner_;
public:
	application(): guard_(ctx_.get_executor()), runner_([this] { ctx_.run(); }) {}
	application(const application&) = delete;
	application(application&&) = delete;
	application& operator=(const application&) = delete;
	application& operator=(application&&) = delete;
	~application()
	{
		guard_.reset();
		runner_.join();
	}
	boost::asio::io_context& context() { return ctx_; }
};

void main_impl(int argc, char** argv)
{
	if (argc != 3)
	{
		std::cerr << "Usage: " << argv[0] << " <username> <password>\n";
		exit(1);
	}

	// Context and connections
	application app; // boost::asio::io_context and a thread that calls run()
	boost::mysql::tcp_connection conn (app.context());

	boost::asio::ip::tcp::endpoint ep (
		boost::asio::ip::address_v4::loopback(), // host
		boost::mysql::default_port			     // port
	);
	boost::mysql::connection_params params (
		argv[1],               // username
		argv[2],		       // password
		"mysql_asio_examples"  // database to use; leave empty or omit the parameter for no database
	);


	// TCP connect
	std::future<void> fut = conn.next_layer().async_connect(ep, use_future);
	fut.get();

	/**
	 * Perform the MySQL handshake. Calling async_handshake triggers the
	 * operation, and calling future::get() blocks the current thread until
	 * it completes. get() will throw an exception if the operation fails.
	 *
	 * For compatibility with other async methods, futures may return an
	 * error_info object. However, this would only contain information
	 * in case of error, and in that case get() would throw. Thus,
	 * the returned error_info is always empty.
	 */
	std::future<boost::mysql::error_info> fut2 =
			conn.async_handshake(params, use_future);
	fut2.get();


	/**
	 * Issue the query to the server. The returned value is an async_handler_arg<tcp_resultset>,
	 * which is a resultset plus an error_info, which is also empty.
	 */
	const char* sql = "SELECT first_name, last_name, salary FROM employee WHERE company_id = 'HGS'";
	std::future<boost::mysql::async_handler_arg<boost::mysql::tcp_resultset>> resultset_fut =
			conn.async_query(sql, use_future);

	// First get() is for the future, second is for the async_handler_arg
	boost::mysql::tcp_resultset result = resultset_fut.get().get();

	/**
	 * Get all rows in the resultset. We will employ resultset::async_fetch_one(),
	 * which returns a single row at every call. The returned row is a pointer
	 * to memory owned by the resultset, and is re-used for each row. Thus, returned
	 * rows remain valid until the next call to async_fetch_one(). When no more
	 * rows are available, async_fetch_one returns nullptr.
	 */
	while (const boost::mysql::row* current_row = result.async_fetch_one(use_future).get().get())
	{
		print_employee(*current_row);
	}

	// application dtor. stops io_context and then joins the thread
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
