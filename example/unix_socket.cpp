//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "boost/mysql/mysql.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/system/system_error.hpp>
#include <iostream>

/**
 * For this example, we will be using the 'boost_mysql_examples' database.
 * You can get this database by running db_setup.sql.
 * This example assumes you are connecting to MySQL server using
 * a UNIX socket. The socket path can be configured using command line
 * arguments, and defaults to /var/run/mysqld/mysqld.sock
 *
 * This example uses synchronous functions and handles errors using exceptions.
 */

void print_employee(const boost::mysql::row& employee)
{
    std::cout << "Employee '"
              << employee.values()[0] << " "                   // first_name (type std::string_view)
              << employee.values()[1] << "' earns "            // last_name  (type std::string_view)
              << employee.values()[2] << " dollars yearly\n";  // salary     (type double)
}

// UNIX sockets are only available in, er, UNIX systems. Typedefs for
// UNIX socket-based connections are only available in UNIX systems.
// Check for BOOST_ASIO_HAS_LOCAL_SOCKETS to know if UNIX socket
// typedefs are available in your system.
#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS

void main_impl(int argc, char** argv)
{
    if (argc != 3 && argc != 4)
    {
        std::cerr << "Usage: " << argv[0] << " <username> <password> [<socket-path>]\n";
        exit(1);
    }

    const char* socket_path = "/var/run/mysqld/mysqld.sock";
    if (argc == 4)
    {
        socket_path = argv[3];
    }

    /**
     * Connection parameters that tell us where and how to connect to the MySQL server.
     * There are two types of parameters:
     *   - UNIX-level connection parameters, identifying the UNIX socket to connect to.
     *   - MySQL level parameters: database credentials and schema to use.
     */
    boost::asio::local::stream_protocol::endpoint ep (socket_path);
    boost::mysql::connection_params params (
        argv[1],               // username
        argv[2],               // password
        "boost_mysql_examples" // database to use; leave empty or omit the parameter for no database
    );
    // Note: by default, SSL will be used if the server supports it.
    // connection_params accepts an optional ssl_options argument
    // determining whether to use SSL or not. See ssl_options and ssl_mode
    // documentation for further details on SSL.

    boost::asio::io_context ctx;

    // Connection to the MySQL server, over a UNIX socket
    boost::mysql::unix_connection conn (ctx);
    conn.connect(ep, params); // UNIX socket connect and MySQL handshake

    const char* sql = "SELECT first_name, last_name, salary FROM employee WHERE company_id = 'HGS'";
    boost::mysql::unix_resultset result = conn.query(sql);

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
    [[maybe_unused]] double salary = rows[0].values()[0].get<double>();
    assert(salary == 10000);

    // Notify the MySQL server we want to quit, then close the underlying connection.
    conn.close();
}

#else

void main_impl(int, char**)
{
    std::cout << "Sorry, your system does not support UNIX sockets" << std::endl;
}

#endif

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

