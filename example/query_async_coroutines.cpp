//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//[example_query_async_coroutines

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/mysql.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/system/system_error.hpp>
#include <boost/asio/spawn.hpp>
#include <iostream>

using boost::mysql::error_code;
using boost::mysql::error_info;

void print_employee(const boost::mysql::row& employee)
{
    std::cout << "Employee '"
              << employee.values()[0] << " "                   // first_name (type boost::string_view)
              << employee.values()[1] << "' earns "            // last_name  (type boost::string_view)
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
    if (argc != 4)
    {
        std::cerr << "Usage: " << argv[0] << " <username> <password> <server-hostname>\n";
        exit(1);
    }

    const char* hostname = argv[3];

    // I/O context and connection. We use SSL because MySQL 8+ default settings require it.
    boost::asio::io_context ctx;
    boost::asio::ssl::context ssl_ctx (boost::asio::ssl::context::tls_client);
    boost::mysql::tcp_ssl_connection conn (ctx, ssl_ctx);

    // Connection params
    boost::mysql::connection_params params (
        argv[1],               // username
        argv[2],               // password
        "boost_mysql_examples" // database to use; leave empty or omit the parameter for no database
    );

    // Resolver for hostname resolution
    boost::asio::ip::tcp::resolver resolver (ctx.get_executor());


    /**
     * The entry point. We spawn a stackful coroutine using boost::asio::spawn.
     *
     * The coroutine will actually start running when we call io_context::run().
     * It will suspend every time we call one of the asynchronous functions, saving
     * all information it needs for resuming. When the asynchronous operation completes,
     * the coroutine will resume in the point it was left.
     *
     * The return type of a coroutine is the second argument to the handler signature
     * for the asynchronous operation. For example, connection::query has a handler
     * signature of void(error_code, resultset<Stream>), so the coroutine return
     * type is resultset<Stream>.
     *
     */
    boost::asio::spawn(ctx.get_executor(), [&conn, &resolver, params, hostname](boost::asio::yield_context yield) {
        // This error_code and error_info will be filled if an
        // operation fails. We will check them for every operation we perform.
        boost::mysql::error_code ec;
        boost::mysql::error_info additional_info;

        // Hostname resolution
        auto endpoints = resolver.async_resolve(hostname, boost::mysql::default_port_string, yield[ec]);
        check_error(ec);

        // Connect to server
        conn.async_connect(*endpoints.begin(), params, additional_info, yield[ec]);
        check_error(ec);

        // Issue the query to the server
        const char* sql = "SELECT first_name, last_name, salary FROM employee WHERE company_id = 'HGS'";
        boost::mysql::tcp_ssl_resultset result = conn.async_query(sql, additional_info, yield[ec]);
        check_error(ec, additional_info);

        /**
          * Get all rows in the resultset. We will employ resultset::async_read_one(),
          * which reads a single row at every call. The row is read in-place, preventing
          * unnecessary copies. resultset::async_read_one() returns true if a row has been
          * read, false if no more rows are available or an error occurred.
         */
        boost::mysql::row row;
        while (result.async_read_one(row, additional_info, yield[ec]))
        {
            check_error(ec, additional_info);
            print_employee(row);
        }

        // Notify the MySQL server we want to quit, then close the underlying connection.
        conn.async_close(additional_info, yield[ec]);
        check_error(ec, additional_info);
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

//]
