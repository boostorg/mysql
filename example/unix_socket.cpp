//
// Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//[example_unix_socket

#include "boost/mysql/mysql.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/system/system_error.hpp>
#include <iostream>

void print_employee(const boost::mysql::row& employee)
{
    std::cout << "Employee '"
              << employee.values()[0] << " "                   // first_name (type boost::string_view)
              << employee.values()[1] << "' earns "            // last_name  (type boost::string_view)
              << employee.values()[2] << " dollars yearly\n";  // salary     (type double)
}

#define ASSERT(expr) \
    if (!(expr)) \
    { \
        std::cerr << "Assertion failed: " #expr << std::endl; \
        exit(1); \
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

    boost::asio::io_context ctx;

    // Connection to the MySQL server, over a UNIX socket
    boost::mysql::unix_connection conn (ctx);
    conn.connect(ep, params); // UNIX socket connect and MySQL handshake

    const char* sql = "SELECT first_name, last_name, salary FROM employee WHERE company_id = 'HGS'";
    boost::mysql::unix_resultset result = conn.query(sql);

    // Get all the rows in the resultset
    std::vector<boost::mysql::row> employees = result.read_all();
    for (const auto& employee: employees)
    {
        print_employee(employee);
    }

    // We can issue any SQL statement, not only SELECTs. In this case, the returned
    // resultset will have no fields and no rows
    sql = "UPDATE employee SET salary = 10000 WHERE first_name = 'Underpaid'";
    result = conn.query(sql);
    ASSERT(result.fields().size() == 0); // fields() returns a vector containing metadata about the query fields

    // Check we have updated our poor intern salary
    result = conn.query("SELECT salary FROM employee WHERE first_name = 'Underpaid'");
    auto rows = result.read_all();
    ASSERT(rows.size() == 1);
    double salary = rows[0].values()[0].get<double>();
    ASSERT(salary == 10000);

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

//]
