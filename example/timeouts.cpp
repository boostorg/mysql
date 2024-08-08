//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//[example_timeouts

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/row_view.hpp>
#include <boost/mysql/tcp_ssl.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancel_after.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>

#include <chrono>
#include <exception>
#include <iostream>

#if defined(BOOST_ASIO_HAS_CO_AWAIT)

using boost::mysql::error_code;

void print_employee(boost::mysql::row_view employee)
{
    std::cout << "Employee '" << employee.at(0) << " "   // first_name (string)
              << employee.at(1) << "' earns "            // last_name  (string)
              << employee.at(2) << " dollars yearly\n";  // salary     (double)
}

/**
 * We use Boost.Asio's cancel_after completion token to cancel operations
 * after a certain time has elapsed. This is not something specific to Boost.MySQL, and
 * can be used with any other asynchronous operation that follows Asio's model.
 * If the operation times out, it will fail with a boost::asio::error::operation_aborted
 * error code.
 *
 * If any of the MySQL specific operations result in a timeout, the connection is left
 * in an unspecified state. You should close it and re-open it to get it working again.
 */
boost::asio::awaitable<void> coro_main(
    boost::mysql::tcp_ssl_connection& conn,
    boost::asio::ip::tcp::resolver& resolver,
    const boost::mysql::handshake_params& params,
    const char* hostname,
    const char* company_id
)
{
    using boost::asio::cancel_after;
    constexpr std::chrono::seconds timeout(8);

    // TODO: thrown exceptions don't contain diagnostics.
    // Should be solved by https://github.com/boostorg/mysql/issues/329

    // Resolve hostname
    auto endpoints = co_await resolver
                         .async_resolve(hostname, boost::mysql::default_port_string, cancel_after(timeout));

    // Connect to server
    co_await conn.async_connect(*endpoints.begin(), params, cancel_after(timeout));

    // We will be using company_id, which is untrusted user input, so we will use a prepared
    // statement.
    boost::mysql::statement stmt = co_await conn.async_prepare_statement(
        "SELECT first_name, last_name, salary FROM employee WHERE company_id = ?",
        cancel_after(timeout)
    );

    // Execute the statement
    boost::mysql::results result;
    co_await conn.async_execute(stmt.bind(company_id), result, cancel_after(timeout));

    // Print all the obtained rows
    for (boost::mysql::row_view employee : result.rows())
    {
        print_employee(employee);
    }

    // Notify the MySQL server we want to quit, then close the underlying connection.
    co_await conn.async_close(cancel_after(timeout));
}

void main_impl(int argc, char** argv)
{
    if (argc != 4 && argc != 5)
    {
        std::cerr << "Usage: " << argv[0] << " <username> <password> <server-hostname> [company-id]\n";
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

    // Connection parameters
    boost::mysql::handshake_params params(
        argv[1],                // username
        argv[2],                // password
        "boost_mysql_examples"  // database to use; leave empty or omit for no database
    );

    // Resolver for hostname resolution
    boost::asio::ip::tcp::resolver resolver(ctx.get_executor());

    // The entry point. We pass in a function returning a boost::asio::awaitable<void>, as required.
    boost::asio::co_spawn(
        ctx.get_executor(),
        [&conn, &resolver, params, hostname, company_id] {
            return coro_main(conn, resolver, params, hostname, company_id);
        },
        // If any exception is thrown in the coroutine body, rethrow it.
        [](std::exception_ptr ptr) {
            if (ptr)
            {
                std::rethrow_exception(ptr);
            }
        }
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
    catch (const boost::mysql::error_with_diagnostics& err)
    {
        // You will only get this type of exceptions if you use throw_on_error.
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
