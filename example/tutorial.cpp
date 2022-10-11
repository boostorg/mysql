//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/connection.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/mysql.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/system/system_error.hpp>
#include <iostream>
#include <string>

/**
 * For this example, we will be using the 'boost_mysql_examples' database.
 * You can get this database by running db_setup.sql.
 * This example assumes you are connecting to a localhost MySQL server.
 *
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
    // The execution context, required to run I/O operations
    boost::asio::io_context ctx;

    // The SSL context, required to establish TLS connections.
    // The default SSL options are good enough for us at this point.
    boost::asio::ssl::context ssl_ctx (boost::asio::ssl::context::tls_client);

    // The object defining the connection to the MySQL server.
    boost::mysql::tcp_ssl_connection conn (ctx.get_executor(), ssl_ctx);
    //]

    //[tutorial_connect
    // Resolve the hostname to get a collection of endpoints    
    boost::asio::ip::tcp::resolver resolver (ctx.get_executor());
    auto endpoints = resolver.resolve(argv[3], boost::mysql::default_port_string);

    // The username and password to use
    boost::mysql::connection_params params (
        argv[1], // username
        argv[2]  // password
    );

    // Connect to the server using the first endpoint returned by the resolver
    conn.connect(*endpoints.begin(), params);
    //]


    //[tutorial_query
    const char* sql = "SELECT \"Hello world!\"";
    boost::mysql::tcp_ssl_resultset result = conn.query(sql);
    //]

    //[tutorial_read
    std::vector<boost::mysql::row> employees = result.read_all();
    //]

    //[tutorial_values
    const boost::mysql::row& first_row = employees.at(0);
    boost::mysql::field_view first_value = first_row.values().at(0);
    std::cout << first_value << std::endl;
    //]

    //[tutorial_close
    conn.close();
    //]
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
