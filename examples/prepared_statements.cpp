
#include "mysql/connection.hpp"
#include <boost/asio/io_service.hpp>
#include <boost/system/system_error.hpp>
#include <iostream>

/**
 * For this example, we will be using the 'mysql_asio_examples' database.
 * You can get this database by running db_setup.sql.
 * This example assumes you are connecting to a localhost MySQL server.
 *
 * This example uses synchronous functions and handles errors using exceptions.
 *
 * This example assumes you are already familiar with the basic concepts
 * of mysql-asio (tcp_connection, resultset, rows, values). If you are not,
 * please have a look to the query_sync.cpp example.
 */

void main_impl(int argc, char** argv)
{
	if (argc != 3)
	{
		std::cerr << "Usage: " << argv[0] << " <username> <password>\n";
		exit(1);
	}

	// Connection parameters
	boost::asio::ip::tcp::endpoint ep (
		boost::asio::ip::address_v4::loopback(), // host
		mysql::default_port					     // port
	);
	mysql::connection_params params (
		argv[1],               // username
		argv[2],		       // password
		"mysql_asio_examples"  // database to use; leave empty or omit the parameter for no database
	);

	boost::asio::io_context ctx;

	// Declare the connection object and authenticate to the server
	mysql::tcp_connection conn (ctx);
	conn.next_level().connect(ep); // next_level() returns a boost::asio::ip::tcp::socket
	conn.handshake(params); // Authenticates to the MySQL server

	/**
	 * We can tell MySQL to prepare a statement using connection::prepare_statement.
	 * We provide a string SQL statement, which can include any number of parameters,
	 * identified by question marks. Parameters are optional: you can prepare a statement
	 * with no parameters.
	 *
	 * Prepared statements are stored in the server on a per-connection basis.
	 * Once a connection is closed, all prepared statements for that connection are deallocated.
	 *
	 * The result of prepare_statement is a mysql::prepared_statement object, which is
	 * templatized on the stream type of the connection (tcp_prepared_statement in our case).
	 *
	 * We prepare two statements, a SELECT and an UPDATE.
	 */
	const char* salary_getter_sql = "SELECT salary FROM employee WHERE first_name = ?";
	mysql::tcp_prepared_statement salary_getter = conn.prepare_statement(salary_getter_sql);
	assert(salary_getter.num_params() == 1); // num_params() returns the number of parameters (question marks)

	const char* salary_updater_sql = "UPDATE employee SET salary = ? WHERE first_name = ?";
	mysql::tcp_prepared_statement salary_updater = conn.prepare_statement(salary_updater_sql);
	assert(salary_updater.num_params() == 2);

	/*
	 * Once a statement has been prepared, it can be executed as many times as
	 * desired, by calling prepared_statement::execute(). execute takes as input a
	 * (possibly empty) collection of mysql::value's and returns a resultset.
	 * The returned resultset works the same as the one returned by connection::query().
	 *
	 * The parameters passed to execute() are replaced in order of declaration:
	 * the first question mark will be replaced by the first passed parameter,
	 * the second question mark by the second parameter and so on. The number
	 * of passed parameters must match exactly the number of parameters for
	 * the prepared statement.
	 *
	 * Any collection providing member functions begin() and end() returning
	 * forward iterators to mysql::value's is acceptable. We use mysql::make_values(),
	 * which creates a std::array with the passed in values converted to mysql::value's.
	 * An iterator version of execute() is also available.
	 */
	mysql::tcp_resultset result = salary_getter.execute(mysql::make_values("Efficient"));
	std::vector<mysql::owning_row> salaries = result.fetch_all(); // Get all the results
	assert(salaries.size() == 1);
	[[maybe_unused]] auto salary = std::get<double>(salaries[0].values().at(0)); // First row, first column
	assert(salary == 30000);
	std::cout << "The salary before the payrise was: " << salary << std::endl;

	/**
	 * Run the update. In this case, we must pass in two parameters.
	 * Note that MySQL is flexible in the types passed as parameters.
	 * In this case, we are sending the value 35000, which gets converted
	 * into a mysql::value with type std::int32_t, while the 'salary'
	 * column is declared as a DOUBLE. The MySQL server will do
	 * the right thing for us.
	 */
	salary_updater.execute(mysql::make_values(35000, "Efficient"));

	/**
	 * Execute the select again. We can execute a prepared statement
	 * as many times as we want. We do NOT need to call
	 * connection::prepare_statement() again.
	 */
	result = salary_getter.execute(mysql::make_values("Efficient"));
	salaries = result.fetch_all();
	assert(salaries.size() == 1);
	salary = std::get<double>(salaries[0].values().at(0));
	assert(salary == 35000); // Our update took place, and the dev got his pay rise
	std::cout << "The salary after the payrise was: " << salary << std::endl;

	/**
	 * Close the statements. Closing a statement deallocates it from the server.
	 * Once a statement is closed, trying to execute it will return an error.
	 *
	 * Closing statements implies communicating with the server and can thus fail.
	 *
	 * Statements are automatically deallocated once the connection is closed.
	 * If you are re-using connection objects and preparing statements over time,
	 * you should close() your statements to prevent excessive resource usage.
	 * If you are not re-using the connections, or are preparing your statements
	 * just once at application startup, there is no need to perform this step.
	 */
	salary_updater.close();
	salary_getter.close();
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
