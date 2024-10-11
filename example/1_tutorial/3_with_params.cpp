//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// TODO: link this

#include <boost/asio/awaitable.hpp>
#ifdef BOOST_ASIO_HAS_CO_AWAIT

//[bla

#include <boost/mysql/any_address.hpp>
#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/with_params.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/this_coro.hpp>

#include <cstdint>
#include <exception>
#include <iostream>
#include <string>

namespace mysql = boost::mysql;
namespace asio = boost::asio;

/**
 * This example shows how to issue queries with parameters containing
 * untrusted input securely. Given an employee ID, it prints their first name.
 * The example builds on the previous async tutorial.
 */
asio::awaitable<void> coro_main(
    std::string server_hostname,
    std::string username,
    std::string password,
    std::int64_t employee_id
)
{
    // Represents a connection to the MySQL server.
    // The connection will use the same executor as the coroutine
    mysql::any_connection conn(co_await asio::this_coro::executor);

    // The hostname, username, password and database to use.
    mysql::connect_params params{
        .server_address = mysql::host_and_port{std::move(server_hostname)},
        .username = std::move(username),
        .password = std::move(password),
        .database = "boost_mysql_examples"
    };

    // Connect to the server
    co_await conn.async_connect(params);

    // Execute the query with the given parameters. When executed, with_params
    // expands the given query string template and sends it to the server for execution.
    // {} are placeholders, as in std::format. Values are escaped as required to prevent
    // SQL injection.
    mysql::results result;
    co_await conn.async_execute(
        mysql::with_params("SELECT first_name FROM employee WHERE id = {}", employee_id),
        result
    );

    // Did we find an employee with that ID?
    if (result.rows().empty())
    {
        std::cout << "Employee not found" << std::endl;
    }
    else
    {
        // Print the first field in the first row
        std::cout << "Employee's name is: " << result.rows().at(0).at(0) << std::endl;
    }

    // Close the connection
    co_await conn.async_close();
}

void main_impl(int argc, char** argv)
{
    if (argc != 5)
    {
        std::cerr << "Usage: " << argv[0] << " <username> <password> <server-hostname> <employee-id>\n";
        exit(1);
    }

    // The execution context, required to run I/O operations.
    asio::io_context ctx;

    // The entry point. We pass in a function returning
    // boost::asio::awaitable<void>, as required.
    // Calling co_spawn enqueues the coroutine for execution.
    asio::co_spawn(
        ctx,
        [argv] { return coro_main(argv[3], argv[1], argv[2], std::stoi(argv[4])); },
        // If any exception is thrown in the coroutine body, rethrow it.
        [](std::exception_ptr ptr) {
            if (ptr)
            {
                std::rethrow_exception(ptr);
            }
        }
    );

    // Calling run will actually execute the coroutine until completion
    ctx.run();
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

#else

#include <iostream>

int main()
{
    std::cout << "Sorry, your compiler doesn't have the required capabilities to run this example"
              << std::endl;
}

#endif