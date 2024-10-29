//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//[example_tutorial_sync

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>
#include <boost/mysql/results.hpp>

#include <boost/asio/io_context.hpp>

#include <iostream>

namespace mysql = boost::mysql;
namespace asio = boost::asio;

/**
 * This example uses the 'boost_mysql_examples' database, which you
 * can get by running db_setup.sql.
 * This example uses synchronous functions and handles errors using exceptions.
 */

void main_impl(int argc, char** argv)
{
    if (argc != 4)
    {
        std::cerr << "Usage: " << argv[0] << " <username> <password> <server-hostname>\n";
        exit(1);
    }

    //[tutorial_connection
    // The execution context, required to run I/O operations.
    asio::io_context ctx;

    // Represents a connection to the MySQL server.
    mysql::any_connection conn(ctx);
    //]

    //[tutorial_connect
    // The hostname, username, password and database to use
    mysql::connect_params params;
    params.server_address.emplace_host_and_port(argv[3]);
    params.username = argv[1];
    params.password = argv[2];
    params.database = "boost_mysql_examples";

    // Connect to the server
    conn.connect(params);
    //]

    //[tutorial_query
    // Issue the SQL query to the server
    const char* sql = "SELECT 'Hello world!'";
    mysql::results result;
    conn.execute(sql, result);
    //]

    //[tutorial_results
    // Print the first field in the first row
    std::cout << result.rows().at(0).at(0) << std::endl;
    //]

    //[tutorial_close
    // Close the connection
    conn.close();
    //]
}

int main(int argc, char** argv)
{
    try
    {
        main_impl(argc, argv);
    }
    catch (const mysql::error_with_diagnostics& err)
    {
        // Some errors include additional diagnostics, like server-provided error messages.
        // Security note: diagnostics::server_message may contain user-supplied values (e.g. the
        // field value that caused the error) and is encoded using to the connection's character set
        // (UTF-8 by default). Treat is as untrusted input.
        std::cerr << "Error: " << err.what() << '\n'
                  << "Server diagnostics: " << err.get_diagnostics().server_message() << std::endl;
        return 1;
    }
    catch (const std::exception& err)
    {
        std::cerr << "Error: " << err.what() << std::endl;
        return 1;
    }
}

//]
