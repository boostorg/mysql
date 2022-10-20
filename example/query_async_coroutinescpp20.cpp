//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//[example_query_async_coroutinescpp20

#include <boost/mysql.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/tcp_ssl.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/system/system_error.hpp>

#include <exception>
#include <iostream>

using boost::mysql::error_code;

#ifdef BOOST_ASIO_HAS_CO_AWAIT

void print_employee(boost::mysql::row_view employee)
{
    std::cout << "Employee '" << employee[0] << " "   // first_name (string)
              << employee[1] << "' earns "            // last_name  (string)
              << employee[2] << " dollars yearly\n";  // salary     (double)
}

/**
 * Our coroutine. It must have a return type of boost::asio::awaitable<T>.
 * Our coroutine does not communicate any result back, so T=void.
 * Remember that you do not have to explicitly create any awaitable<void> in
 * your function. Instead, the return type is fed to std::coroutine_traits
 * to determine the semantics of the coroutine, like the promise type.
 * Asio already takes care of all this for us.
 *
 * The coroutine will suspend every time we call one of the asynchronous functions, saving
 * all information it needs for resuming. When the asynchronous operation completes,
 * the coroutine will resume in the point it was left.
 *
 * The return type of an asynchronous operation that uses boost::asio::use_awaitable
 * as completion token is a boost::asio::awaitable<T>, where T
 * is the second argument to the handler signature for the asynchronous operation.
 * If any of the asynchronous operations fail, an exception will be raised
 * within the coroutine.
 */
boost::asio::awaitable<void> start_query(
    boost::mysql::tcp_ssl_connection& conn,
    boost::asio::ip::tcp::resolver& resolver,
    const boost::mysql::handshake_params& params,
    const char* hostname
)
{
    try
    {
        // Resolve hostname
        auto endpoints = co_await resolver.async_resolve(
            hostname,
            boost::mysql::default_port_string,
            boost::asio::use_awaitable
        );

        // Connect to server
        co_await conn.async_connect(*endpoints.begin(), params, boost::asio::use_awaitable);

        /**
         * Issue the query to the server. Note that async_query returns a
         * boost::asio::awaitable<boost::mysql::tcp_ssl_resultset>.
         */
        const char*
            sql = "SELECT first_name, last_name, salary FROM employee WHERE company_id = 'HGS'";
        boost::mysql::tcp_ssl_resultset result;
        co_await conn.async_query(sql, result, boost::asio::use_awaitable);

        /**
         * Get all rows in the resultset. We will employ resultset::async_read_one(),
         * which reads a single row at every call. resultset::complete() returns true only after
         * we've read the last row in the resultset.
         */
        boost::mysql::row row;
        while (co_await result.async_read_one(row, boost::asio::use_awaitable))
        {
            print_employee(row);
        }

        // Notify the MySQL server we want to quit, then close the underlying connection.
        co_await conn.async_close(boost::asio::use_awaitable);
    }
    catch (const boost::system::system_error& err)
    {
        std::cerr << "Error: " << err.what() << ", error code: " << err.code() << std::endl;
    }
    catch (const std::exception& err)
    {
        std::cerr << "Error: " << err.what() << std::endl;
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
    boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::tls_client);
    boost::mysql::tcp_ssl_connection conn(ctx, ssl_ctx);

    // Connection parameters
    boost::mysql::handshake_params params(
        argv[1],                // username
        argv[2],                // password
        "boost_mysql_examples"  // database to use; leave empty or omit the parameter for no
                                // database
    );

    // Resolver for hostname resolution
    boost::asio::ip::tcp::resolver resolver(ctx.get_executor());

    /**
     * The entry point. We pass in a function returning
     * a boost::asio::awaitable<void>, as required.
     */
    boost::asio::co_spawn(
        ctx.get_executor(),
        [&conn, &resolver, params, hostname] {
            return start_query(conn, resolver, params, hostname);
        },
        boost::asio::detached
    );

    // Calling run will actually start the requested operations.
    ctx.run();
}

#else

void main_impl(int, char**)
{
    std::cout << "Sorry, your compiler does not support C++20 coroutines" << std::endl;
}

#endif

int main(int argc, char** argv)
{
    try
    {
        main_impl(argc, argv);
    }
    catch (const std::exception& err)
    {
        std::cerr << "Error: " << err.what() << std::endl;
        return 1;
    }
}

//]
