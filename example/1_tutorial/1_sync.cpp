//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

/**
 * Creates a connection, establishes a session and
 * runs a simple "Hello world!" query.
 *
 * This example uses synchronous functions and handles errors using exceptions.
 */

#include <boost/mysql/any_address.hpp>
#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/results.hpp>

#include <boost/capy/ex/run_async.hpp>
#include <boost/capy/ex/this_coro.hpp>
#include <boost/capy/io_task.hpp>
#include <boost/corosio/io_context.hpp>

#include <iostream>

namespace mysql = boost::mysql;
namespace corosio = boost::corosio;
namespace capy = boost::capy;

static void check_err(capy::io_result<mysql::diagnostics> result)
{
    if (result.ec)
    {
        std::cerr << "Error connecting: " << result.ec << ": " << result.get<1>().server_message()
                  << std::endl;
        exit(1);
    }
}

capy::io_task<> main_impl()
{
    // if (argc != 4)
    // {
    //     std::cerr << "Usage: " << argv[0] << " <username> <password> <server-hostname>\n";
    //     exit(1);
    // }

    mysql::any_connection conn(co_await capy::this_coro::executor);

    mysql::connect_params params{
        .server_address = mysql::host_and_port("localhost"),
        .username = "root",
        .password = "",
    };

    // Connect to the server
    check_err(co_await conn.connect(params));

    // Issue the SQL query to the server
    const char* sql = "SELECT 'Hello world!'";
    mysql::results result;
    check_err(co_await conn.execute(sql, result));

    // Print the first field in the first row
    std::cout << result.rows().at(0).at(0) << std::endl;

    // // Close the connection
    // conn.close();

    co_return {};
}

int main(int argc, char** argv)
{
    corosio::io_context ctx;
    capy::run_async(ctx.get_executor())(main_impl());
    ctx.run();
}

//]
