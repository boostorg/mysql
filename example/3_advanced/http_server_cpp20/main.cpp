//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/awaitable.hpp>
#include <boost/pfr/config.hpp>
#if defined(BOOST_ASIO_HAS_CO_AWAIT) && BOOST_PFR_CORE_NAME_ENABLED

//[example_http_server_cpp20_main_cpp

/**
 * Implements a HTTP REST API using Boost.MySQL and Boost.Beast.
 * The API models a simplified order management system for an online store.
 * Using the API, users can query the store's product catalog, create and
 * edit orders, and check them out for payment.
 *
 * The API defines the following endpoints:
 *
 *    GET    /products?search={s}       Returns a list of products
 *    GET    /orders                    Returns all orders
 *    GET    /orders?id={}              Returns a single order
 *    POST   /orders                    Creates a new order
 *    POST   /orders/items              Adds a new order item to an existing order
 *    DELETE /orders/items?id={}        Deletes an order item
 *    POST   /orders/checkout?id={}     Checks out an order
 *    POST   /orders/complete?id={}     Completes an order
 *
 * Each order can have any number of order items. An order item
 * represents an individual product that has been added to an order.
 * Orders are created empty, in a 'draft' state. Items can then be
 * added and removed from the order. After adding the desired items,
 * orders can be checked out for payment. A third-party service, like Stripe,
 * would be used to collect the payment. For simplicity, we've left this part
 * out of the example. Once checked out, an order is no longer editable.
 * Finally, after successful payment, order are transitioned to the
 * 'complete' status.
 *
 * The server uses C++20 coroutines and is multi-threaded.
 * It also requires linking to Boost::json and Boost::url.
 * The database schema is defined in db_setup.sql, in the same directory as this file.
 * You need to source this file before running the example.
 */

#include <boost/mysql/any_address.hpp>
#include <boost/mysql/connection_pool.hpp>
#include <boost/mysql/pool_params.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/system/error_code.hpp>

#include <cstddef>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

#include "server.hpp"

using namespace orders;
namespace mysql = boost::mysql;
namespace asio = boost::asio;

// The number of threads to use
static constexpr std::size_t num_threads = 5;

int main_impl(int argc, char* argv[])
{
    // Check command line arguments.
    if (argc != 5)
    {
        std::cerr << "Usage: " << argv[0] << " <username> <password> <mysql-hostname> <port>\n";
        return EXIT_FAILURE;
    }

    // Application config
    const char* mysql_username = argv[1];
    const char* mysql_password = argv[2];
    const char* mysql_hostname = argv[3];
    auto port = static_cast<unsigned short>(std::stoi(argv[4]));

    // An event loop, where the application will run.
    // We will use the main thread to run the pool, too, so we use
    // one thread less than configured
    asio::thread_pool th_pool(num_threads - 1);

    // Create a connection pool
    mysql::connection_pool pool(
        // Use the thread pool as execution context
        th_pool,

        // Pool configuration
        mysql::pool_params{
            // Connect using TCP, to the given hostname and using the default port
            .server_address = mysql::host_and_port{mysql_hostname},

            // Authenticate using the given username
            .username = mysql_username,

            // Password for the above username
            .password = mysql_password,

            // Database to use when connecting
            .database = "boost_mysql_orders",

            // We're using multi-queries
            .multi_queries = true,

            // Using thread_safe will make the pool thread-safe by internally
            // creating and using a strand.
            // This allows us to share the pool between sessions, which may run
            // concurrently, on different threads.
            .thread_safe = true,
        }
    );

    // Launch the MySQL pool
    pool.async_run(asio::detached);

    // A signal_set allows us to intercept SIGINT and SIGTERM and
    // exit gracefully
    asio::signal_set signals{th_pool.get_executor(), SIGINT, SIGTERM};

    // Capture SIGINT and SIGTERM to perform a clean shutdown
    signals.async_wait([&th_pool](boost::system::error_code, int) {
        // Stop the execution context. This will cause main to exit
        th_pool.stop();
    });

    // Start listening for HTTP connections. This will run until the context is stopped
    asio::co_spawn(
        // Use the thread pool to run the listener coroutine
        th_pool,

        // The coroutine to run
        [&pool, port] { return run_server(pool, port); },

        // If an exception is thrown in the listener coroutine, propagate it
        [](std::exception_ptr exc) {
            if (exc)
                std::rethrow_exception(exc);
        }
    );

    // Attach the current thread to the thread pool. This will block
    // until stop() is called
    th_pool.attach();

    // Wait until all threads have exited
    th_pool.join();

    std::cout << "Server exiting" << std::endl;

    // (If we get here, it means we got a SIGINT or SIGTERM)
    return EXIT_SUCCESS;
}

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

#else

#include <iostream>

int main()
{
    std::cout << "Sorry, your compiler doesn't have the required capabilities to run this example"
              << std::endl;
}

#endif
