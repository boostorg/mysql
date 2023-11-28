//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/any_address.hpp>
#include <boost/mysql/connection_pool.hpp>
#include <boost/mysql/pool_params.hpp>

#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/system/detail/error_code.hpp>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include "repository.hpp"
#include "server.hpp"

// This example implements a minimal REST API to manage notes.
// A note is a simple object containing a user-defined title and content.
// The REST API offers CRUD operations on such objects:
//    POST   /notes        Creates a new note.
//    GET    /notes        Retrieves all notes.
//    GET    /notes/<id>   Retrieves a single note.
//    PUT    /notes/<id>   Replaces a note, changing its title and content.
//    DELETE /notes/<id>   Deletes a note.
//
// Notes are stored in MySQL. The note_repository class encapsulates
//   access to MySQL, offering friendly functions to manipulate notes.
// server.cpp encapsulates all the boilerplate to launch an HTTP server,
//   match URLs to API endpoints, and invoke the relevant note_repository functions.
// All communication happens asynchronously. We use stackful coroutines to simplify
//   development, using boost::asio::spawn and boost::asio::yield_context.

using namespace notes;

int main(int argc, char* argv[])
{
    // Check command line arguments.
    if (argc != 4)
    {
        std::cerr << "Usage: " << argv[0] << " <username> <password> <mysql-hostname>\n";
        return EXIT_FAILURE;
    }

    // Application config
    const char* mysql_username = argv[1];
    const char* mysql_password = argv[2];
    const char* mysql_hostname = argv[3];

    // An event loop, where the application will run. The server is single-
    // threaded, so we set the concurrency hint to 1
    boost::asio::io_context ioc{1};

    // Configuration for the connection pool
    boost::mysql::pool_params pool_prms{
        // Connect using TCP, to the given hostname and using the default port
        boost::mysql::host_and_port(mysql_hostname),

        // Authenticate using the given username
        mysql_username,

        // Password for the above username
        mysql_password,

        // Database to use when connecting
        "boost_mysql_examples",
    };
    auto shared_st = std::make_shared<shared_state>(boost::mysql::connection_pool(ioc, std::move(pool_prms)));

    // A signal_set allows us to intercept SIGINT and SIGTERM and
    // exit gracefully
    boost::asio::signal_set signals{ioc.get_executor(), SIGINT, SIGTERM};

    // Launch the MySQL pool
    shared_st->pool.async_run(boost::asio::detached);

    // Start listening for HTTP connections. This will run until the context is stopped
    auto ec = launch_server(ioc, shared_st);
    if (ec)
    {
        std::cerr << "Error launching server: " << ec << std::endl;
        exit(EXIT_FAILURE);
    }

    // Capture SIGINT and SIGTERM to perform a clean shutdown
    signals.async_wait([shared_st, &ioc](boost::system::error_code, int) {
        // Stop the connection pool. This will cause
        shared_st->pool.cancel();

        // Stop the io_context. This will cause run() to return
        ioc.stop();
    });

    // Run the io_context. This will block until the context is stopped by
    // a signal and all outstanding async tasks are finished.
    ioc.run();

    // (If we get here, it means we got a SIGINT or SIGTERM)
    return EXIT_SUCCESS;
}
