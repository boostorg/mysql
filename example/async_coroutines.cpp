//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//[example_async_coroutines

#include <boost/mysql.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/system/system_error.hpp>

#include <iostream>

using boost::mysql::error_code;
using boost::mysql::error_info;

void print_employee(boost::mysql::row_view employee)
{
    std::cout << "Employee '" << employee.at(0) << " "   // first_name (string)
              << employee.at(1) << "' earns "            // last_name  (string)
              << employee.at(2) << " dollars yearly\n";  // salary     (double)
}

// Throws an exception if an operation failed
void check_error(const error_code& err, const error_info& info = {})
{
    if (err)
        throw boost::system::system_error(err, info.message());
}

void main_impl(int argc, char** argv)
{
    if (argc != 4 && argc != 5)
    {
        std::cerr << "Usage: " << argv[0]
                  << " <username> <password> <server-hostname> [company-id]\n";
        exit(1);
    }

    const char* hostname = argv[3];

    // The company_id whose employees we will be listing. This
    // is user-supplied input, and should be treated as untrusted.
    const char* company_id = argc == 5 ? argv[4] : "HGS";

    // I/O context and connection. We use SSL because MySQL 8+ default settings require it.
    boost::asio::io_context ctx;
    boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::tls_client);
    boost::mysql::tcp_ssl_connection conn(ctx, ssl_ctx);

    // Connection params
    boost::mysql::handshake_params params(
        argv[1],                // username
        argv[2],                // password
        "boost_mysql_examples"  // database to use; leave empty or omit for no database
    );

    // Resolver for hostname resolution
    boost::asio::ip::tcp::resolver resolver(ctx.get_executor());

    /**
     * The entry point. We spawn a stackful coroutine using boost::asio::spawn.
     *
     * The coroutine will actually start running when we call io_context::run().
     * It will suspend every time we call one of the asynchronous functions, saving
     * all information it needs for resuming. When the asynchronous operation completes,
     * the coroutine will resume in the point it was left.
     */
    boost::asio::spawn(
        ctx.get_executor(),
        [&conn, &resolver, params, hostname, company_id](boost::asio::yield_context yield) {
            // This error_code and error_info will be filled if an
            // operation fails. We will check them for every operation we perform.
            boost::mysql::error_code ec;
            boost::mysql::error_info additional_info;

            // Hostname resolution
            auto endpoints = resolver.async_resolve(
                hostname,
                boost::mysql::default_port_string,
                yield[ec]
            );
            check_error(ec);

            // Connect to server
            conn.async_connect(*endpoints.begin(), params, additional_info, yield[ec]);
            check_error(ec);

            // We will be using company_id, which is untrusted user input, so we will use a prepared
            // statement.
            boost::mysql::tcp_ssl_statement stmt;
            conn.async_prepare_statement(
                "SELECT first_name, last_name, salary FROM employee WHERE company_id = ?",
                stmt,
                additional_info,
                yield[ec]
            );
            check_error(ec, additional_info);

            // Execute the statement
            boost::mysql::resultset result;
            stmt.async_execute(std::make_tuple(company_id), result, additional_info, yield[ec]);
            check_error(ec, additional_info);

            // Print the employees
            for (boost::mysql::row_view employee : result.rows())
            {
                print_employee(employee);
            }

            // Notify the MySQL server we want to quit, then close the underlying connection.
            conn.async_close(additional_info, yield[ec]);
            check_error(ec, additional_info);
        }
    );

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
