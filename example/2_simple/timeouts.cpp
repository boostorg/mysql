//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/awaitable.hpp>
#ifdef BOOST_ASIO_HAS_CO_AWAIT

//[example_timeouts

/**
 * This example demonstrates how to set a timeout to your async operations
 * using asio::cancel_after. We will set a timeout to an individual query,
 * as well as to an entire coroutine. cancel_after can be used with any
 * Boost.Asio-compliant async function.
 *
 * This example uses C++20 coroutines. If you need, you can backport
 * it to C++11 by using callbacks or asio::yield_context.
 * Timeouts can't be used with sync functions.
 */

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/row_view.hpp>
#include <boost/mysql/with_params.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancel_after.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>

#include <chrono>
#include <exception>
#include <iostream>
#include <string_view>

namespace asio = boost::asio;
namespace mysql = boost::mysql;

void print_employee(mysql::row_view employee)
{
    std::cout << "Employee '" << employee.at(0) << " "   // first_name (string)
              << employee.at(1) << "' earns "            // last_name  (string)
              << employee.at(2) << " dollars yearly\n";  // salary     (double)
}

// The main coroutine
asio::awaitable<void> coro_main(
    std::string_view server_hostname,
    std::string_view username,
    std::string_view password,
    std::string_view company_id
)
{
    // Create a connection.
    // Will use the same executor as the coroutine.
    mysql::any_connection conn(co_await asio::this_coro::executor);

    // The hostname, username, password and database to use
    mysql::connect_params params;
    params.server_address.emplace_host_and_port(std::string(server_hostname));
    params.username = username;
    params.password = password;
    params.database = "boost_mysql_examples";

    // Connect to server
    co_await conn.async_connect(params);

    // Execute the query. company_id is untrusted, so we use with_params.
    // We set a timeout to this query by using asio::cancel_after.
    // On timeout, the operation will fail with asio::error::operation_aborted.
    // You can use asio::cancel_after with any async operation.
    // After a timeout happens, the connection needs to be re-connected.
    mysql::results result;
    co_await conn.async_execute(
        mysql::with_params(
            "SELECT first_name, last_name, salary FROM employee WHERE company_id = {}",
            company_id
        ),
        result,
        asio::cancel_after(std::chrono::seconds(5))
    );

    // Print all the obtained rows
    for (boost::mysql::row_view employee : result.rows())
    {
        print_employee(employee);
    }

    // Notify the MySQL server we want to quit, then close the underlying connection.
    co_await conn.async_close();
}

void main_impl(int argc, char** argv)
{
    if (argc != 5)
    {
        std::cerr << "Usage: " << argv[0] << " <username> <password> <server-hostname> <company-id>\n";
        exit(1);
    }

    // Create an I/O context, required by all I/O objects
    asio::io_context ctx;

    // Launch our coroutine with a timeout.
    // If the entire operation hasn't finished before the timeout,
    // the operation being executed at that point will get cancelled,
    // and the entire coroutine will fail with asio::error::operation_aborted
    asio::co_spawn(
        ctx,
        [=] { return coro_main(argv[3], argv[1], argv[2], argv[4]); },
        asio::cancel_after(
            std::chrono::seconds(20),
            [](std::exception_ptr ptr) {
                if (ptr)
                {
                    std::rethrow_exception(ptr);
                }
            }
        )
    );

    // Calling run will actually execute the coroutine until completion
    ctx.run();

    std::cout << "Done\n";
}

int main(int argc, char** argv)
{
    try
    {
        main_impl(argc, argv);
    }
    catch (const boost::mysql::error_with_diagnostics& err)
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