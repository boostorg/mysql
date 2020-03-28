
#include "boost/mysql/connection.hpp"
#include <boost/asio/io_service.hpp>
#include <boost/system/system_error.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/yield.hpp>
#include <iostream>

using boost::mysql::error_code;
using boost::mysql::error_info;
using boost::mysql::async_handler_arg;
using boost::mysql::tcp_resultset;
using boost::mysql::owning_row;

/**
 * For this example, we will be using the 'mysql_asio_examples' database.
 * You can get this database by running db_setup.sql.
 * This example assumes you are connecting to a localhost MySQL server.
 *
 * This example uses asynchronous functions with callbacks.
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

void die_on_error(
	const error_code& err,
	const boost::mysql::error_info& info = boost::mysql::error_info()
)
{
	if (err)
	{
		std::cerr << "Error: " << err << ": " << info.message() << std::endl;
		exit(1);
	}
}

class application
{
	boost::asio::ip::tcp::endpoint ep;
	boost::mysql::connection_params conn_params;
	boost::asio::io_context ctx;
	boost::mysql::tcp_connection connection;
	boost::mysql::tcp_resultset resultset;
public:
	application(const char* username, const char* password) :
		ep (boost::asio::ip::address_v4::loopback(), boost::mysql::default_port),
		conn_params(username, password, "mysql_asio_examples"),
		connection(ctx)
	{
	}

	void start() { connect(); }

	void connect()
	{
		connection.next_layer().async_connect(ep, [this](error_code err) {
			die_on_error(err);
			connection.async_handshake(conn_params, [this](error_code err, const error_info& info) {
				die_on_error(err, info);
				query_employees();
			});
		});
	}

	void query_employees()
	{
		const char* sql = "SELECT first_name, last_name, salary FROM employee WHERE company_id = 'HGS'";
		connection.async_query(sql, [this](error_code err,
										   async_handler_arg<tcp_resultset>&& result
			) {
			die_on_error(err, result.error());
			resultset = std::move(result.get());
			resultset.async_fetch_all([this](error_code err,
										     const async_handler_arg<std::vector<owning_row>>& rows) {
				die_on_error(err, rows.error());
				for (const auto& employee: rows.get())
				{
					print_employee(employee);
				}
				update_slacker();
			});
		});
	}

	void update_slacker()
	{
		const char* sql = "UPDATE employee SET salary = 15000 WHERE last_name = 'Slacker'";
		connection.async_query(sql, [this](error_code err,
				                           async_handler_arg<tcp_resultset>&& result) {
			die_on_error(err, result.error());
			assert(result.get().fields().size() == 0);
			query_intern();
		});
	}

	void query_intern()
	{
		const char* sql = "SELECT salary FROM employee WHERE last_name = 'Slacker'";
		connection.async_query(sql, [this](error_code err,
										   async_handler_arg<tcp_resultset>&& result) {
			die_on_error(err, result.error());
			resultset = std::move(result.get());
			resultset.async_fetch_all([](error_code err,
										 const async_handler_arg<std::vector<owning_row>>& rows) {
				die_on_error(err, rows.error());
				assert(rows.get().size() == 1);
				[[maybe_unused]] auto salary = std::get<double>(rows.get()[0].values()[0]);
				assert(salary == 15000);
			});
		});
	}

	auto& context() { return ctx; }
};

void main_impl(int argc, char** argv)
{
	if (argc != 3)
	{
		std::cerr << "Usage: " << argv[0] << " <username> <password>\n";
		exit(1);
	}

	application app (argv[1], argv[2]);
	app.start(); // starts the async chain
	app.context().run(); // run the asio::io_context until the async chain finishes
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
