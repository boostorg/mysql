//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/pfr.hpp>

#include <boost/asio/awaitable.hpp>
#if defined(BOOST_ASIO_HAS_CO_AWAIT) && BOOST_PFR_CORE_NAME_ENABLED

//[example_tutorial_error_handling

/**
 * This tutorial adds error handling to the program in the previous tutorial.
 * It shows how to avoid exceptions and use diagnostics objects.
 *
 * It uses Boost.Pfr for reflection, which requires C++20.
 * You can backport it to C++14 if you need by using Boost.Describe.
 * It uses C++20 coroutines. If you need, you can backport
 * it to C++11 by using callbacks, asio::yield_context
 * or sync functions instead of coroutines.
 *
 * This example uses the 'boost_mysql_examples' database, which you
 * can get by running db_setup.sql.
 */

#include <boost/mysql/connection_pool.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/pfr.hpp>
#include <boost/mysql/pool_params.hpp>
#include <boost/mysql/static_results.hpp>
#include <boost/mysql/with_params.hpp>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/cancel_after.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/write.hpp>
#include <boost/endian/conversion.hpp>
#include <boost/system/error_code.hpp>

#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string>

namespace mysql = boost::mysql;
namespace asio = boost::asio;

//[tutorial_error_handling_log_error
// Log an error to std::cerr
void log_error(const char* header, boost::system::error_code ec, const mysql::diagnostics& diag = {})
{
    // Inserting the error code only prints the number and category. Add the message, too.
    std::cerr << header << ": " << ec << " " << ec.message();

    // client_message() contains client-side generated messages that don't
    // contain user-input. This is usually embedded in exceptions.
    // When working with error codes, we need to log it explicitly
    if (!diag.client_message().empty())
    {
        std::cerr << ": " << diag.client_message();
    }

    // server_message() contains server-side messages, and thus may
    // contain user-supplied input. Printing it is safe.
    if (!diag.server_message().empty())
    {
        std::cerr << ": " << diag.server_message();
    }

    // Done
    std::cerr << '\n';
}
//]

// Should contain a member for each field of interest present in our query
struct employee
{
    std::string first_name;
    std::string last_name;
};

// Encapsulates the database access logic.
// Given an employee_id, retrieves the employee details to be sent to the client.
//[tutorial_error_handling_db
asio::awaitable<std::string> get_employee_details(mysql::connection_pool& pool, std::int64_t employee_id)
{
    // Will be populated with error information in case of error
    mysql::diagnostics diag;

    // Get a connection from the pool.
    // This will wait until a healthy connection is ready to be used.
    // pooled_connection grants us exclusive access to the connection until
    // the object is destroyed
    //[tutorial_error_handling_structured_bindings
    // ec is an error_code, conn is the mysql::pooled_connection
    auto [ec, conn] = co_await pool.async_get_connection(diag, asio::as_tuple);
    //]
    if (ec)
    {
        log_error("Error in async_get_connection", ec, diag);
        co_return "ERROR";
    }

    // Use the connection normally to query the database.
    // operator-> returns a reference to an any_connection,
    // so we can apply all what we learnt in previous tutorials
    mysql::static_results<mysql::pfr_by_name<employee>> result;
    auto [ec2] = co_await conn->async_execute(
        mysql::with_params("SELECT first_name, last_name FROM employee WHERE id = {}", employee_id),
        result,
        diag,
        asio::as_tuple
    );
    if (ec2)
    {
        log_error("Error running query", ec, diag);
        co_return "ERROR";
    }

    // Compose the message to be sent back to the client
    if (result.rows().empty())
    {
        co_return "NOT_FOUND";
    }
    else
    {
        const auto& emp = result.rows()[0];
        co_return emp.first_name + ' ' + emp.last_name;
    }

    // When the pooled_connection is destroyed, the connection is returned
    // to the pool, so it can be re-used.
}
//]

//[tutorial_error_handling_session
asio::awaitable<void> handle_session(mysql::connection_pool& pool, asio::ip::tcp::socket client_socket)
{
    // Read the request from the client.
    // async_read ensures that the 8-byte buffer is filled, handling partial reads.
    unsigned char message[8]{};
    auto [ec1, bytes_read] = co_await asio::async_read(client_socket, asio::buffer(message), asio::as_tuple);
    if (ec1)
    {
        log_error("Error reading from the socket", ec1);
        co_return;
    }

    // Parse the 64-bit big-endian int into a native int64_t
    std::int64_t employee_id = boost::endian::load_big_s64(message);

    // Invoke the database handling logic
    std::string response = co_await get_employee_details(pool, employee_id);

    // Write the response back to the client.
    // async_write ensures that the entire message is written, handling partial writes
    auto [ec2, bytes_written] = co_await asio::async_write(
        client_socket,
        asio::buffer(response),
        asio::as_tuple
    );
    if (ec2)
    {
        log_error("Error writing to the socket", ec2);
        co_return;
    }

    // The socket's destructor will close the client connection
}
//]

asio::awaitable<void> listener(mysql::connection_pool& pool, unsigned short port)
{
    // An object that accepts incoming TCP connections.
    asio::ip::tcp::acceptor acc(co_await asio::this_coro::executor);

    // The endpoint where the server will listen.
    asio::ip::tcp::endpoint listening_endpoint(asio::ip::make_address("0.0.0.0"), port);

    // Open the acceptor
    acc.open(listening_endpoint.protocol());

    // Allow reusing the local address, so we can restart our server
    // without encountering errors in bind
    acc.set_option(asio::socket_base::reuse_address(true));

    // Bind to the local address
    acc.bind(listening_endpoint);

    // Start listening for connections
    acc.listen();
    std::cout << "Server listening at " << acc.local_endpoint() << std::endl;

    // Start the accept loop
    while (true)
    {
        // Accept a new connection
        auto [ec, sock] = co_await acc.async_accept(asio::as_tuple);
        if (ec)
        {
            log_error("Error accepting connection", ec);
            co_return;
        }

        // Launch a coroutine that runs our session logic.
        // We don't co_await this coroutine so we can listen
        // to new connections while the session is running
        asio::co_spawn(
            // Use the same executor as the current coroutine
            co_await asio::this_coro::executor,

            // Session logic. Take ownership of the socket
            [&pool, sock = std::move(sock)]() mutable { return handle_session(pool, std::move(sock)); },

            // Completion token for the coroutine.
            // If the coroutine hasn't finished after 60 seconds, Asio will cancel
            // any I/O operation the coroutine is waiting for.
            // The coroutine will see a failure in the I/O operation it's waiting
            // for and throw, as it would for a network error.
            // The supplied callback will be executed when the coroutine
            // completes, even if it's cancelled.
            asio::cancel_after(
                std::chrono::seconds(60),
                [](std::exception_ptr ex) {
                    if (ex)
                        std::rethrow_exception(ex);
                }
            )
        );
    }
}

void main_impl(int argc, char** argv)
{
    if (argc != 5)
    {
        std::cerr << "Usage: " << argv[0] << " <username> <password> <server-hostname> <listener-port>\n";
        exit(1);
    }

    const char* username = argv[1];
    const char* password = argv[2];
    const char* server_hostname = argv[3];

    // Create an I/O context, required by all I/O objects
    asio::io_context ctx;

    // pool_params contains configuration for the pool.
    // You must specify enough information to establish a connection,
    // including the server address and credentials.
    // You can configure a lot of other things, like pool limits
    mysql::pool_params params;
    params.server_address.emplace_host_and_port(server_hostname);
    params.username = username;
    params.password = password;
    params.database = "boost_mysql_examples";

    // Construct the pool.
    // ctx will be used to create the connections and other I/O objects
    mysql::connection_pool pool(ctx, std::move(params));

    // You need to call async_run on the pool before doing anything useful with it.
    // async_run creates connections and keeps them healthy. It must be called
    // only once per pool.
    // The detached completion token means that we don't want to be notified when
    // the operation ends. It's similar to a no-op callback.
    pool.async_run(asio::detached);

    // signal_set is an I/O object that allows waiting for signals
    asio::signal_set signals(ctx, SIGINT, SIGTERM);

    // Wait for signals
    signals.async_wait([&](boost::system::error_code, int) {
        // Stop the execution context. This will cause io_context::run to return
        ctx.stop();
    });

    // Launch our listener
    asio::co_spawn(
        ctx,
        [&pool, argv] { return listener(pool, std::stoi(argv[4])); },
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
