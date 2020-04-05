
#include "boost/mysql/connection.hpp"
#include <boost/asio/io_service.hpp>
#include <boost/system/system_error.hpp>
#include <iostream>

/**
 * For this example, we will be using the 'mysql_asio_examples' database.
 * You can get this database by running db_setup.sql.
 * This example assumes you are connecting to a localhost MySQL server.
 *
 * This example uses synchronous functions and handles errors using exceptions.
 */

/**
 * Prints an employee to std::cout. An employee here is a mysql::row,
 * which represents a row returned by a SQL query. You can access the values in
 * the row using row::values(), which returns a vector of mysql::value.
 *
 * mysql::value represents a single value returned by MySQL, and is defined to be
 * a std::variant of all the types MySQL supports.
 *
 * row::values() has the same number of elements as fields are in the SQL query,
 * and in the same order.
 */
void print_employee(const boost::mysql::row& employee)
{
	using boost::mysql::operator<<; // Required for mysql::value objects to be streamable, due to ADL rules
	std::cout << "Employee '"
			  << employee.values()[0] << " "                   // first_name (type std::string_view)
			  << employee.values()[1] << "' earns "            // last_name  (type std::string_view)
			  << employee.values()[2] << " dollars yearly\n";  // salary     (type double)
}

void main_impl(int argc, char** argv)
{
	if (argc != 3)
	{
		std::cerr << "Usage: " << argv[0] << " <username> <password>\n";
		exit(1);
	}

	/**
	 * Connection parameters that tell us where and how to connect to the MySQL server.
	 * There are two types of parameters:
	 *   - TCP-level connection parameters, identifying the host and port to connect to.
	 *   - MySQL level parameters: database credentials and schema to use.
	 */
	boost::asio::ip::tcp::endpoint ep (
		boost::asio::ip::address_v4::loopback(), // host
		boost::mysql::default_port					     // port
	);
	boost::mysql::connection_params params (
		argv[1],               // username
		argv[2],		       // password
		"mysql_asio_examples"  // database to use; leave empty or omit the parameter for no database
	);
	// connection_params accepts an optional ssl_options argument describing
	// whether to use TLS on the connection or not. By default, TLS is enabled
	// and required.

	boost::asio::io_context ctx;

	/**
	 * Represents a single connection over TCP to a MySQL server.
	 * Before being able to use it, you have to connect to the server by:
	 *    - Establishing the TCP-level session.
	 *    - Authenticating to the MySQL server.
	 */
	boost::mysql::tcp_connection conn (ctx);
	conn.next_layer().connect(ep); // next_level() returns a boost::asio::ip::tcp::socket
	conn.handshake(params); // Authenticates to the MySQL server

	/**
	 * To issue a SQL query to the database server, use tcp_connection::query, which takes
	 * the SQL to be executed as parameter and returns a resultset object.
	 *
	 * Resultset objects represent the result of a query, in tabular format.
	 * They hold metadata describing the fields the resultset holds (in this case, first_name,
	 * last_name and salary). To get the actual data, use fetch_one, fetch_many or fetch_all.
	 * We will use fetch_all, which returns all the received rows as a std::vector.
	 *
	 * We will get all employees working for 'High Growth Startup'.
	 */
	const char* sql = "SELECT first_name, last_name, salary FROM employee WHERE company_id = 'HGS'";
	boost::mysql::tcp_resultset result = conn.query(sql);

	// Get all the rows in the resultset
	std::vector<boost::mysql::owning_row> employees = result.fetch_all();
	for (const auto& employee: employees)
	{
		print_employee(employee);
	}

	// We can issue any SQL statement, not only SELECTs. In this case, the returned
	// resultset will have no fields and no rows
	sql = "UPDATE employee SET salary = 10000 WHERE first_name = 'Underpaid'";
	result = conn.query(sql);
	assert(result.fields().size() == 0); // fields() returns a vector containing metadata about the query fields

	// Check we have updated our poor intern salary
	result = conn.query("SELECT salary FROM employee WHERE first_name = 'Underpaid'");
	auto rows = result.fetch_all();
	assert(rows.size() == 1);
	[[maybe_unused]] auto salary = std::get<double>(rows[0].values()[0]);
	assert(salary == 10000);
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
