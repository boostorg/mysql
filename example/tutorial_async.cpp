//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// TODO: link this

#include <boost/asio/awaitable.hpp>

//<-
#ifdef BOOST_ASIO_HAS_CO_AWAIT
//->

#include <boost/mysql/any_address.hpp>
#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>
#include <boost/mysql/results.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/this_coro.hpp>

#include <exception>
#include <iostream>

namespace mysql = boost::mysql;
namespace asio = boost::asio;

/**
 * This example is analogous to the synchronous tutorial, but uses async functions
 * with C++20 coroutines, instead. It uses the 'boost_mysql_examples' database.
 * You can get this database by running db_setup.sql.
 *
 * This function implements the main coroutine.
 * It must have a return type of boost::asio::awaitable<T>.
 * Our coroutine does not communicate any result back, so T=void.
 *
 * The coroutine will suspend every time we call one of the asynchronous functions, saving
 * all information it needs for resuming. When the asynchronous operation completes,
 * the coroutine will resume in the point it was left.
 * We use the same program structure as in the sync world, replacing
 * sync functions by their async equivalents and adding co_await in front of them.
 */
asio::awaitable<void> coro_main(std::string server_hostname, std::string username, std::string password)
{
    // Represents a connection to the MySQL server.
    mysql::any_connection conn(co_await asio::this_coro::executor);

    // The hostname, username, password and database to use
    mysql::connect_params params{
        .server_address = mysql::host_and_port{std::move(server_hostname)},
        .username = std::move(username),
        .password = std::move(password),
        .database = "boost_mysql_examples"
    };

    // Connect to the server
    co_await conn.async_connect(params);

    // Issue the SQL query to the server
    const char* sql = "SELECT 'Hello world!'";
    mysql::results result;
    co_await conn.async_execute(sql, result);

    // Print the first field in the first row
    std::cout << result.rows().at(0).at(0) << std::endl;

    // Close the connection
    co_await conn.async_close();
}

void main_impl(int argc, char** argv)
{
    if (argc != 4)
    {
        std::cerr << "Usage: " << argv[0] << " <username> <password> <server-hostname>\n";
        exit(1);
    }

    // The execution context, required to run I/O operations.
    asio::io_context ctx;

    // The entry point. We pass in a function returning
    // boost::asio::awaitable<void>, as required.
    // Calling co_spawn enqueues the coroutine for execution.
    asio::co_spawn(
        ctx,
        [argv] { return coro_main(argv[3], argv[1], argv[2]); },
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

//<-
#else

void main_impl(int, char**)
{
    std::cout << "Sorry, your compiler does not support C++20 coroutines" << std::endl;
}

#endif
//->

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
