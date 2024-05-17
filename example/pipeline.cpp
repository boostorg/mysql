//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//[example_any_connection

// any_connection is a connection type that is easier to use than regular
// connection. It is type-erased: it's not a template, and is able to connect
// to any server using TCP, UNIX sockets and SSL. It features a simplified
// connect and async_connect function family, which handle name resolution.
// Performance is equivalent to regular connection.
//
// This example demonstrates how to connect to a server using any_connection.
// It uses asynchronous functions and coroutines (with boost::asio::spawn).
// Recall that using these coroutines requires linking against Boost.Context.
//
// any_connection is an experimental feature.

#include <boost/mysql/any_address.hpp>
#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/pipeline.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/row_view.hpp>
#include <boost/mysql/static_pipeline.hpp>
#include <boost/mysql/throw_on_error.hpp>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>

#include <iostream>

using boost::mysql::error_code;

void main_impl(int argc, char** argv)
{
    if (argc != 4 && argc != 5)
    {
        std::cerr << "Usage: " << argv[0] << " <username> <password> <server-hostname> [company-id]\n";
        exit(1);
    }

    const char* hostname = argv[3];

    // The company_id whose employees we will be listing. This
    // is user-supplied input, and should be treated as untrusted.
    const char* company_id = argc == 5 ? argv[4] : "HGS";

    // I/O context
    boost::asio::io_context ctx;

    // Connection. Note that the connection's type doesn't depend
    // on the transport (TCP or UNIX sockets).
    boost::mysql::any_connection conn(ctx);

    // Connection configuration. This contains the server address,
    // credentials, and other configuration used during connection establishment.
    // Note that, by default, TCP connections will use TLS. connect_params::ssl
    // allows disabling it.
    boost::mysql::connect_params params;

    // The server address. This can either be a host and port or a UNIX socket path
    params.server_address.emplace_host_and_port(hostname);

    // Username to log in as
    params.username = argv[1];

    // Password to use
    params.password = argv[2];

    // Database to use; leave empty or omit for no database
    params.database = "boost_mysql_examples";

    /**
     * The entry point. We spawn a stackful coroutine using boost::asio::spawn.
     *
     * The coroutine will actually start running when we call io_context::run().
     * It will suspend every time we call one of the asynchronous functions, saving
     * all information it needs for resuming. When the asynchronous operation completes,
     * the coroutine will resume in the point it was left.
     */
    boost::asio::co_spawn(
        ctx.get_executor(),
        [&conn, &params, company_id]() -> boost::asio::awaitable<void> {
            using namespace boost::mysql;
            constexpr auto tok = boost::asio::as_tuple(boost::asio::deferred);

            // This error_code and diagnostics will be filled if an
            // operation fails. We will check them for every operation we perform.
            boost::mysql::diagnostics diag;

            // Connect to the server. This will take care of resolving the provided
            // hostname to an IP address, connect to that address, and establish
            // the MySQL session.
            auto [ec1] = co_await conn.async_connect(params, diag, tok);
            boost::mysql::throw_on_error(ec1, diag);

            // Prepare pipeline
            boost::mysql::static_pipeline<
                boost::mysql::execute_step,
                boost::mysql::prepare_statement_step,
                boost::mysql::prepare_statement_step>
                pipe{
                    "START TRANSACTION",
                    "INSERT INTO employee (company_id, first_name, last_name) VALUES (?, ?, ?)",
                    "INSERT INTO audit_log (msg) VALUES (?)"
                };

            // Execute
            auto [ec2] = co_await conn.async_run_pipeline(pipe, tok);
            boost::mysql::throw_on_error(ec2, diag);

            // Extract results
            auto stmt1 = std::get<1>(pipe.steps()).result();
            auto stmt2 = std::get<2>(pipe.steps()).result();

            // Prepare the pipeline
            static_pipeline<
                execute_step,
                execute_step,
                execute_step,
                close_statement_step,
                close_statement_step>
                pipe2{
                    {stmt1, make_stmt_params("HGS", "Juan", "Lopez")},
                    {stmt2, make_stmt_params("Inserted new emplyee")},
                    "COMMIT",
                    stmt1,
                    stmt2
            };

            // Run it again
            auto [ec3] = co_await conn.async_run_pipeline(pipe2, tok);
            boost::mysql::throw_on_error(ec3, diag);

            auto id = std::get<0>(pipe2.steps()).result().last_insert_id();
            std::cout << "Inserted employee: " << id << std::endl;

            // Notify the MySQL server we want to quit, then close the underlying connection.
            auto [ec4] = co_await conn.async_close(diag, tok);
            boost::mysql::throw_on_error(ec4, diag);
        },
        // If any exception is thrown in the coroutine body, rethrow it.
        [](std::exception_ptr ptr) {
            if (ptr)
            {
                std::rethrow_exception(ptr);
            }
        }
    );

    // Don't forget to call run()! Otherwise, your program
    // will not spawn the coroutine and will do nothing.
    ctx.run();
}

int main(int argc, char** argv)
{
    try
    {
        main_impl(argc, argv);
    }
    catch (const boost::mysql::error_with_diagnostics& err)
    {
        // You will only get this type of exceptions if you use throw_on_error.
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
