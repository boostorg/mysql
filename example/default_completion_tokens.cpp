//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//[example_default_completion_tokens]

#include <boost/mysql.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/basic_stream_socket.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/system/system_error.hpp>

#include <iostream>

using boost::mysql::error_code;

#ifdef BOOST_ASIO_HAS_CO_AWAIT

void print_employee(boost::mysql::row_view employee)
{
    std::cout << "Employee '" << employee.at(0) << " "   // first_name (string)
              << employee.at(1) << "' earns "            // last_name  (string)
              << employee.at(2) << " dollars yearly\n";  // salary     (double)
}

/**
 * Default completion tokens are associated to executors.
 * boost::mysql::connection objects use the same executor
 * as the underlying stream (socket). boost::mysql::tcp_ssl_connection
 * objects use boost::asio::ssl::stream<boost::asio::ip::tcp::socket>,
 * which use the polymorphic boost::asio::executor as executor type,
 * which does not have a default completion token associated.
 *
 * We will use the io_context's executor as base executor. We will then
 * use use_awaitable_t::executor_with_default on this type, which creates
 * a new executor type acting the same as the base executor, but having
 * use_awaitable_t as default completion token type.
 *
 * We will then obtain the connection type to use by rebinding
 * the usual tcp_ssl_connection to our new executor type, coro_executor_type.
 *
 * The reward for this hard work is not having to pass the completion
 * token (boost::asio::use_awaitable) to any of the asynchronous operations
 * initiated by this connection or any of the I/O objects (e.g. resultsets)
 * associated to them.
 */
using base_executor_type = boost::asio::io_context::executor_type;
using coro_executor_type = boost::asio::use_awaitable_t<base_executor_type>::executor_with_default<
    base_executor_type>;
using stream_type = boost::asio::ssl::stream<
    boost::asio::basic_stream_socket<boost::asio::ip::tcp, coro_executor_type> >;
using connection_type = boost::mysql::connection<stream_type>;
using resolver_type = boost::asio::ip::basic_resolver<boost::asio::ip::tcp, coro_executor_type>;

// Our coroutine
boost::asio::awaitable<void, base_executor_type> start_query(
    connection_type& conn,
    resolver_type& resolver,
    const char* hostname,
    const boost::mysql::handshake_params& params,
    const char* company_id
)
{
    try
    {
        // Resolve hostname. Note: we didn't have to pass boost::asio::use_awaitable.
        auto endpoints = co_await resolver.async_resolve(
            hostname,
            boost::mysql::default_port_string
        );

        // Connect to server
        co_await conn.async_connect(*endpoints.begin(), params);

        // Prepare an statement. Note that the statement type won't be
        // tcp_ssl_statement, because the stream type we are using is different.
        // We can use connection::statement_type to help
        connection_type::statement_type stmt;
        co_await conn.async_prepare_statement(
            "SELECT first_name, last_name, salary FROM employee WHERE company_id = ?",
            stmt
        );

        // Execute it
        boost::mysql::resultset result;
        co_await stmt.async_execute(std::make_tuple(company_id), result);
        for (boost::mysql::row_view employee : result.rows())
        {
            print_employee(employee);
        }

        // Notify the MySQL server we want to quit, then close the underlying connection.
        // This will also deallocate the statement from the server.
        co_await conn.async_close();
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
    if (argc != 4 && argc != 5)
    {
        std::cerr << "Usage: " << argv[0]
                  << " <username> <password> <server-hostname> [company-id]\n";
        exit(1);
    }

    const char* hostname = argv[3];
    const char* company_id = argc == 5 ? argv[4] : "HGS";

    // I/O context and connection. We use SSL because MySQL 8+ default settings require it.
    boost::asio::io_context ctx;
    boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::tls_client);
    connection_type conn(ctx.get_executor(), ssl_ctx);

    // Resolver, for hostname resolution
    resolver_type resolver(ctx.get_executor());

    // Connection parameters
    boost::mysql::handshake_params params(
        argv[1],                // username
        argv[2],                // password
        "boost_mysql_examples"  // database to use; leave empty or omit for no database
    );

    /**
     * The entry point. We pass in a function returning
     * a boost::asio::awaitable<void>, as required.
     */
    boost::asio::co_spawn(
        ctx.get_executor(),
        [&conn, &resolver, hostname, params, company_id] {
            return start_query(conn, resolver, hostname, params, company_id);
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
