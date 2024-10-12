//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/awaitable.hpp>
#ifdef BOOST_ASIO_HAS_CO_AWAIT

//[example_transactions

/**
 * This program shows how to start, commit and roll back transactions
 * spanning multiple queries.
 *
 * The program modifies an order of an online store system,
 * adding a new line item to it. The program must first check
 * that the order is an editable state.
 *
 * This example uses C++20 coroutines. If you need, you can backport
 * it to C++14 (required by Boost.Describe) by using callbacks, asio::yield_context
 * or sync functions instead of coroutines.
 */

#include <boost/mysql/any_address.hpp>
#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/with_params.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/this_coro.hpp>

#include <cstdint>
#include <iostream>
#include <string>

namespace asio = boost::asio;
namespace mysql = boost::mysql;

// The main coroutine
asio::awaitable<void> coro_main(
    std::string server_hostname,
    std::string username,
    std::string password,
    std::uint64_t order_id,
    std::uint64_t product_id
)
{
    // Create a connection.
    // Will use the same executor as the coroutine.
    mysql::any_connection conn(co_await asio::this_coro::executor);

    // The hostname, username, password and database to use
    mysql::connect_params params{
        .server_address = mysql::host_and_port(std::move(server_hostname)),
        .username = std::move(username),
        .password = std::move(password),
        .database = "boost_mysql_examples",
        .multi_queries = true,  // Enable support for semicolon-separated queries
    };

    // Connect to the server
    co_await conn.async_connect(params);

    // Open a transaction block with START TRANSACTION.
    // Transaction statements are regular SQL, and can be issued using async_execute.
    // Then retrieve the order and lock it so it doesn't get modified while we're examining it.
    // We need to check whether the order is in a 'draft' state before adding items to it.
    // We combine the transaction and the select statements to save round-trips to the server.
    mysql::results result;
    co_await conn.async_execute(
        mysql::with_params(
            "START TRANSACTION; "
            "SELECT status FROM orders WHERE id = {} FOR SHARE",
            order_id
        ),
        result
    );

    // We issued 2 queries, so we get 2 resultsets back.
    // The 1st resultset corresponds to the START TRANSACTION and is empty.
    // The 2nd resultset corresponds to the SELECT and contains our order.
    // If a connection closes while a transaction is in progress,
    // the transaction is rolled back. No ROLLBACK statement required.
    mysql::rows_view orders = result.at(1).rows();
    if (orders.empty())
    {
        std::cout << "Can't find order with id=" << order_id << std::endl;
        exit(1);
    }

    // Retrieve and check the order status
    auto order_status = orders.at(0).at(0).as_string();
    if (order_status != "draft")
    {
        std::cout << "Order can't be modified because it's in " << order_status << " status\n";
        exit(1);
    }

    // We're good to proceed. Insert the new order item and commit the transaction.
    // If the INSERT fails, the COMMIT statement is not executed
    // and the transaction is rolled back when the connection closes.
    co_await conn.async_execute(
        mysql::with_params(
            "INSERT INTO order_items (order_id, product_id, quantity) VALUES ({}, {}, 1); "
            "COMMIT",
            order_id,
            product_id
        ),
        result
    );

    // Notify the MySQL server we want to quit, then close the underlying connection.
    co_await conn.async_close();
}

void main_impl(int argc, char** argv)
{
    if (argc != 6)
    {
        std::cerr << "Usage: " << argv[0]
                  << " <username> <password> <server-hostname> <order-id> <product-id>\n";
        exit(1);
    }

    // Create an I/O context, required by all I/O objects
    asio::io_context ctx;

    // Launch our coroutine
    asio::co_spawn(
        ctx,
        [=] { return coro_main(argv[3], argv[1], argv[2], std::stoi(argv[4]), std::stoi(argv[5])); },
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
