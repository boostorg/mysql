//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//[example_pipeline

// This example demonstrates how to use the pipeline API to prepare,
// execute and close statements in batch.
// It uses asynchronous functions and C++20 coroutines (with boost::asio::co_spawn).
//
// Pipelines are an experimental feature.

#include <boost/mysql/any_address.hpp>
#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/pipeline.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/row_view.hpp>
#include <boost/mysql/string_view.hpp>
#include <boost/mysql/throw_on_error.hpp>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/core/span.hpp>

#include <array>
#include <iostream>
#include <vector>

#ifdef BOOST_ASIO_HAS_CO_AWAIT

namespace asio = boost::asio;

using boost::mysql::error_code;
using boost::mysql::string_view;

// Prepare several statements in batch.
// This is faster than preparing them one by one, as it saves round-trips to the server.
asio::awaitable<std::vector<boost::mysql::statement>> batch_prepare(
    boost::mysql::any_connection& conn,
    boost::span<const string_view> statements
)
{
    // Construct a pipeline request describing the work to be performed.
    // There must be one prepare_statement_stage per statement to prepare
    boost::mysql::pipeline_request req;
    for (auto stmt_sql : statements)
        req.add_prepare_statement(stmt_sql);

    // Run the pipeline. Using as_tuple prevents async_run_pipeline from throwing.
    // This allows us to include the diagnostics object diag in the thrown exception.
    // stage_response is a variant-like type that can hold the response of any stage type.
    std::vector<boost::mysql::stage_response> pipe_res;
    boost::mysql::diagnostics diag;
    auto [ec] = co_await conn.async_run_pipeline(req, pipe_res, diag, asio::as_tuple(asio::deferred));
    boost::mysql::throw_on_error(ec, diag);

    // If we got here, all statements were prepared successfully.
    // pipe_res contains as many elements as statements.size(), holding statement objects
    // Extract them into a vector
    std::vector<boost::mysql::statement> res;
    res.reserve(statements.size());
    for (const auto& stage_res : pipe_res)
        res.push_back(stage_res.get_statement());
    co_return res;
}

void main_impl(int argc, char** argv)
{
    if (argc != 4 && argc != 5)
    {
        std::cerr << "Usage: " << argv[0] << " <username> <password> <server-hostname> [company-id]\n";
        exit(1);
    }

    const char* hostname = argv[3];

    // The company_id to use when inserting new employees.
    // This is user-supplied input, and should be treated as untrusted.
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

    // Spawn a coroutine running the passed function
    boost::asio::co_spawn(
        ctx.get_executor(),
        [&conn, &params, company_id]() -> boost::asio::awaitable<void> {
            // Use as_tuple and throw_on_error to include diagnostics in our exceptions
            constexpr auto tok = boost::asio::as_tuple(boost::asio::deferred);
            boost::mysql::diagnostics diag;

            // Connect to the server
            auto [ec] = co_await conn.async_connect(params, diag, tok);
            boost::mysql::throw_on_error(ec, diag);

            // Prepare the statements using the batch prepare function that we previously defined
            const std::array<string_view, 2> stmt_sql{
                "INSERT INTO employee (company_id, first_name, last_name) VALUES (?, ?, ?)",
                "INSERT INTO audit_log (msg) VALUES (?)"
            };
            std::vector<boost::mysql::statement> stmts = co_await batch_prepare(conn, stmt_sql);

            // Create a pipeline request to execute them.
            // Warning: do NOT include the COMMIT statement in this pipeline.
            // COMMIT must only be executed if all the previous statements succeeded.
            // In a pipeline, all stages get executed, regardless of the outcome of previous stages.
            // We say that COMMIT has a dependency on the result of previous stages.
            boost::mysql::pipeline_request req;
            req.add_execute("START TRANSACTION")
                .add_execute(stmts.at(0), company_id, "Juan", "Lopez")
                .add_execute(stmts.at(0), company_id, "Pepito", "Rodriguez")
                .add_execute(stmts.at(0), company_id, "Someone", "Random")
                .add_execute(stmts.at(1), "Inserted 3 new emplyees");
            std::vector<boost::mysql::stage_response> res;

            // Execute the pipeline
            std::tie(ec) = co_await conn.async_run_pipeline(req, res, diag, tok);
            boost::mysql::throw_on_error(ec, diag);

            // If we got here, all stages executed successfully.
            // Since they were execution stages, the response contains a results object.
            // Get the IDs of the newly created employees
            auto id1 = res.at(1).as_results().last_insert_id();
            auto id2 = res.at(2).as_results().last_insert_id();
            auto id3 = res.at(3).as_results().last_insert_id();

            // We can now commit our transaction and close the statements.
            // Clear the request and populate it again
            req.clear();
            req.add_execute("COMMIT").add_close_statement(stmts.at(0)).add_close_statement(stmts.at(1));

            // Run it
            std::tie(ec) = co_await conn.async_run_pipeline(req, res, diag, tok);
            boost::mysql::throw_on_error(ec, diag);

            // If we got here, our insertions got committed.
            std::cout << "Inserted employees: " << id1 << ", " << id2 << ", " << id3 << std::endl;

            // Notify the MySQL server we want to quit, then close the underlying connection.
            std::tie(ec) = co_await conn.async_close(diag, tok);
            boost::mysql::throw_on_error(ec, diag);
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

#else

int main(int, char**) { std::cout << "Sorry, your compiler does not support C++20 coroutines" << std::endl; }

#endif

//]
