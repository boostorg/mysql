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
#include <string>

#include "repository.hpp"
#include "server.hpp"

using namespace orders;

int main(int argc, char* argv[])
{
    // Check command line arguments.
    if (argc != 4)
    {
        std::cerr << "Usage: " << argv[0] << " <username> <password> <mysql-hostname>\n"
                  << "Example:\n"
                  << "    " << argv[0] << " 0.0.0.0 8080 .\n";
        return EXIT_FAILURE;
    }

    // Application config
    const char* mysql_username = argv[1];
    const char* mysql_password = argv[2];
    const char* mysql_hostname = argv[3];

    // An event loop, where the application will run. The server is single-
    // threaded, so we set the concurrency hint to 1
    boost::asio::io_context ioc{1};

    boost::mysql::pool_params pool_prms{
        boost::mysql::host_and_port(mysql_hostname),
        mysql_username,
        mysql_password,
        "boost_mysql_examples",
    };
    boost::mysql::connection_pool pool(ioc, std::move(pool_prms));
    note_repository repo(pool);

    // A signal_set allows us to intercept SIGINT and SIGTERM and
    // exit gracefully
    boost::asio::signal_set signals{ioc.get_executor(), SIGINT, SIGTERM};

    // Launch the MySQL pool
    pool.async_run(boost::asio::detached);

    // Start listening for HTTP connections. This will run until the context is stopped
    auto ec = launch_server(ioc, 4000, repo);
    if (ec)
    {
        std::cerr << "Error launching server: " << ec << std::endl;
        exit(EXIT_FAILURE);
    }

    // Capture SIGINT and SIGTERM to perform a clean shutdown
    signals.async_wait([&pool, &ioc](boost::system::error_code, int) {
        // Stop the MySQL pool
        pool.cancel();

        // Stop the io_context. This will cause run() to return
        ioc.stop();
    });

    // Run the io_context. This will block until the context is stopped by
    // a signal and all outstanding async tasks are finished.
    ioc.run();

    // (If we get here, it means we got a SIGINT or SIGTERM)
    return EXIT_SUCCESS;
}
