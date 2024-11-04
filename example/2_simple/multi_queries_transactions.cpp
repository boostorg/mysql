//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/awaitable.hpp>
#ifdef BOOST_ASIO_HAS_CO_AWAIT

//[example_multi_queries_transactions

/**
 * This example demonstrates how to use multi-queries
 * to run several semicolon-separated queries in
 * a single async_execute call. It also demonstrates
 * how to use SQL transactions.
 *
 * The program updates the first name of an employee,
 * and prints the employee's full details.
 *
 * It uses C++20 coroutines. If you need, you can backport
 * it to C++11 by using callbacks, asio::yield_context
 * or sync functions instead of coroutines.
 *
 * This example uses the 'boost_mysql_examples' database, which you
 * can get by running db_setup.sql.
 */

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/resultset_view.hpp>
#include <boost/mysql/row_view.hpp>
#include <boost/mysql/with_params.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>

#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>

namespace mysql = boost::mysql;
namespace asio = boost::asio;

// The main coroutine
asio::awaitable<void> coro_main(
    std::string_view server_hostname,
    std::string_view username,
    std::string_view password,
    std::int64_t employee_id,
    std::string_view new_first_name
)
{
    // Create a connection.
    // Will use the same executor as the coroutine.
    mysql::any_connection conn(co_await asio::this_coro::executor);

    //[section_connection_establishment_multi_queries
    // The server host, username, password and database to use.
    // Setting multi_queries to true makes it possible to run several
    // semicolon-separated queries with async_execute.
    mysql::connect_params params;
    params.server_address.emplace_host_and_port(std::string(server_hostname));
    params.username = std::move(username);
    params.password = std::move(password);
    params.database = "boost_mysql_examples";
    params.multi_queries = true;

    // Connect to the server
    co_await conn.async_connect(params);
    //]

    // Perform the update and retrieve the results:
    //   1. Begin a transaction block. Further updates won't be visible to
    //      other transactions until this one commits.
    //   2. Perform the update.
    //   3. Retrieve the employee we just updated. Since we're in a transaction,
    //      the employee record will be locked at this point. This ensures that
    //      we retrieve the employee we updated, and not an employee created
    //      by another transaction. That is, this prevents dirty reads.
    //   4. Commit the transaction and make everything visible to other transactions.
    //      If any of the previous steps fail, the commit won't be run, and the
    //      transaction will be rolled back when the connection is closed.
    mysql::results result;
    co_await conn.async_execute(
        mysql::with_params(
            "START TRANSACTION;"
            "UPDATE employee SET first_name = {1} WHERE id = {0};"
            "SELECT first_name, last_name FROM employee WHERE id = {0};"
            "COMMIT",
            employee_id,
            new_first_name
        ),
        result
    );

    // We've run 4 SQL queries, so MySQL has returned us 4 resultsets.
    // The SELECT is the 3rd resultset. Retrieve it
    mysql::resultset_view select_result = result.at(2);

    // resultset_view has a similar interface to results.
    // Retrieve the generated rows
    if (select_result.rows().empty())
    {
        std::cout << "No employee with ID = " << employee_id << std::endl;
    }
    else
    {
        mysql::row_view employee = select_result.rows().at(0);
        std::cout << "Updated: employee is now " << employee.at(0) << " " << employee.at(1) << std::endl;
    }

    // Notify the MySQL server we want to quit, then close the underlying connection.
    co_await conn.async_close();
}

void main_impl(int argc, char** argv)
{
    if (argc != 6)
    {
        std::cerr << "Usage: " << argv[0]
                  << " <username> <password> <server-hostname> <employee-id> <new-first-name>\n";
        exit(1);
    }

    // Create an I/O context, required by all I/O objects
    asio::io_context ctx;

    // Launch our coroutine
    asio::co_spawn(
        ctx,
        [=] { return coro_main(argv[3], argv[1], argv[2], std::stoi(argv[4]), argv[5]); },
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
        std::cerr << "Error: " << err.what() << ", error code: " << err.code() << '\n'
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