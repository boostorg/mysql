//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/static_results.hpp>
#ifdef BOOST_MYSQL_CXX14

//[example_http_server_cpp14_coroutines_main_cpp

/**
 * Implements a HTTP REST API using Boost.MySQL and Boost.Beast.
 * The server is asynchronous and uses asio::yield_context as its completion
 * style. It only requires C++14 to work.
 *
 * It implements a minimal REST API to manage notes.
 * A note is a simple object containing a user-defined title and content.
 * The REST API offers CRUD operations on such objects:
 *    POST   /notes           Creates a new note.
 *    GET    /notes           Retrieves all notes.
 *    GET    /notes?id=<id>   Retrieves a single note.
 *    PUT    /notes?id=<id>   Replaces a note, changing its title and content.
 *    DELETE /notes?id=<id>   Deletes a note.
 *
 * Notes are stored in MySQL. The note_repository class encapsulates
 * access to MySQL, offering friendly functions to manipulate notes.
 * server.cpp encapsulates all the boilerplate to launch an HTTP server,
 * match URLs to API endpoints, and invoke the relevant note_repository functions.
 *
 * All communication happens asynchronously. We use stackful coroutines to simplify
 * development, using asio::spawn and asio::yield_context.
 * This example requires linking to Boost::context, Boost::json and Boost::url.
 */

#include <boost/mysql/any_address.hpp>
#include <boost/mysql/connection_pool.hpp>
#include <boost/mysql/pool_params.hpp>

#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/system/error_code.hpp>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include "server.hpp"

namespace asio = boost::asio;
namespace mysql = boost::mysql;
using namespace notes;

int main(int argc, char* argv[])
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
    asio::io_context ctx;

    // Configuration for the connection pool
    mysql::pool_params params{
        // Connect using TCP, to the given hostname and using the default port
        mysql::host_and_port{mysql_hostname},

        // Authenticate using the given username
        mysql_username,

        // Password for the above username
        mysql_password,

        // Database to use when connecting
        "boost_mysql_examples",
    };

    // Create the connection pool.
    // shared_state contains all singleton objects that our application may need.
    // Coroutines created by asio::spawn might survive until the io_context is destroyed
    // (even after io_context::stop() has been called). This is not the case for callbacks
    // and C++20 coroutines. Using a shared_ptr here ensures that the pool survives long enough.
    auto st = std::make_shared<shared_state>(mysql::connection_pool(ctx, std::move(params)));

    // Launch the MySQL pool
    st->pool.async_run(asio::detached);

    // A signal_set allows us to intercept SIGINT and SIGTERM and exit gracefully
    asio::signal_set signals{ctx.get_executor(), SIGINT, SIGTERM};
    signals.async_wait([st, &ctx](boost::system::error_code, int) {
        // Stop the execution context. This will cause main to exit
        ctx.stop();
    });

    // Launch the server. This will run until the context is stopped
    asio::spawn(
        // Spawn the coroutine in the io_context
        ctx,

        // The coroutine to run
        [st, port](asio::yield_context yield) { run_server(st, port, yield); },

        // If an exception is thrown in the coroutine, propagate it
        [](std::exception_ptr exc) {
            if (exc)
                std::rethrow_exception(exc);
        }
    );

    // Run the server until stopped
    ctx.run();

    std::cout << "Server exiting" << std::endl;

    // (If we get here, it means we got a SIGINT or SIGTERM)
    return EXIT_SUCCESS;
}

//]

#else

#include <iostream>

int main() { std::cout << "Sorry, your compiler doesn't support C++14\n"; }

#endif
