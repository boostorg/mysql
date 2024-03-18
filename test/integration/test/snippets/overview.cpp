//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/row_view.hpp>
#include <boost/mysql/statement.hpp>
#include <boost/mysql/static_results.hpp>
#include <boost/mysql/tcp_ssl.hpp>
#include <boost/mysql/throw_on_error.hpp>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/test/unit_test.hpp>

#include <string>

#include "test_common/ci_server.hpp"
#include "test_integration/snippets/credentials.hpp"
#include "test_integration/snippets/describe.hpp"
#include "test_integration/snippets/get_connection.hpp"
#include "test_integration/snippets/run_coro.hpp"

#ifdef BOOST_ASIO_HAS_CO_AWAIT
#include <boost/asio/experimental/awaitable_operators.hpp>
#endif

using namespace boost::mysql;
using namespace boost::mysql::test;

// Defined outside the namespace to prevent unused warnings
#if defined(BOOST_ASIO_HAS_CO_AWAIT) && !defined(BOOST_ASIO_USE_TS_EXECUTOR_AS_DEFAULT)
boost::asio::awaitable<void> dont_run()
{
    using namespace boost::asio::experimental::awaitable_operators;

    // Setup
    auto& conn = get_connection();

    //[overview_async_dont
    // Coroutine body
    // DO NOT DO THIS!!!!
    results result1, result2;
    co_await (
        conn.async_execute("SELECT 1", result1, boost::asio::use_awaitable) &&
        conn.async_execute("SELECT 2", result2, boost::asio::use_awaitable)
    );
    //]
}
#endif

namespace {

const char* get_value_from_user() { return ""; }

BOOST_AUTO_TEST_CASE(section_overview)
{
    //[overview_connection
    // The execution context, required to run I/O operations.
    boost::asio::io_context ctx;

    // The SSL context, required to establish TLS connections.
    // The default SSL options are good enough for us at this point.
    boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::tls_client);

    // Represents a connection to the MySQL server.
    boost::mysql::tcp_ssl_connection conn(ctx.get_executor(), ssl_ctx);
    //]

    //[overview_connect
    // Obtain the hostname to connect to - replace get_hostname by your code
    std::string server_hostname = get_hostname();

    // Resolve the hostname to get a collection of endpoints
    boost::asio::ip::tcp::resolver resolver(ctx.get_executor());
    auto endpoints = resolver.resolve(server_hostname, boost::mysql::default_port_string);

    // The username and password to use
    boost::mysql::handshake_params params(
        mysql_username,         // username, as a string
        mysql_password,         // password, as a string - don't hardcode this into your code!
        "boost_mysql_examples"  // database to use
    );

    // Connect to the server using the first endpoint returned by the resolver
    conn.connect(*endpoints.begin(), params);
    //]

    {
        //[overview_query_use_case
        results result;
        conn.execute("START TRANSACTION", result);
        //]
    }
    {
        //[overview_statement_use_case
        statement stmt = conn.prepare_statement(
            "SELECT first_name FROM employee WHERE company_id = ? AND salary > ?"
        );

        results result;
        conn.execute(stmt.bind("HGS", 30000), result);
        //]
    }
    {
        //[overview_ifaces_table
        const char* table_definition = R"%(
            CREATE TEMPORARY TABLE posts (
                id INT PRIMARY KEY AUTO_INCREMENT,
                title VARCHAR (256) NOT NULL,
                body TEXT NOT NULL
            )
        )%";
        //]

        results result;
        conn.execute(table_definition, result);
    }
    {
        //[overview_ifaces_dynamic
        // Passing a results object to connection::execute selects the dynamic interface
        results result;
        conn.execute("SELECT id, title, body FROM posts", result);

        // Every row is a collection of fields, which are variant-like objects
        // that represent data. We use as_string() to cast them to the appropriate type
        for (row_view post : result.rows())
        {
            std::cout << "Title: " << post.at(1).as_string() << "Body: " << post.at(2).as_string()
                      << std::endl;
        }
        //]
    }
#ifdef BOOST_MYSQL_CXX14
    {
        // The struct definition is included above this
        //[overview_ifaces_static
        //
        // This must be placed inside your function or method:
        //

        // Passing a static_results to execute() selects the static interface
        static_results<post> result;
        conn.execute("SELECT id, title, body FROM posts", result);

        // Query results are parsed directly into your own type
        for (const post& p : result.rows())
        {
            std::cout << "Title: " << p.title << "Body: " << p.body << std::endl;
        }
        //]
    }
#endif

    {
        //[overview_statements_setup
        results result;
        conn.execute(
            R"%(
                CREATE TEMPORARY TABLE products (
                    id VARCHAR(50) PRIMARY KEY,
                    description VARCHAR(256)
                )
            )%",
            result
        );
        conn.execute("INSERT INTO products VALUES ('PTT', 'Potatoes'), ('CAR', 'Carrots')", result);
        //]
    }
    {
        //[overview_statements_prepare
        statement stmt = conn.prepare_statement("SELECT description FROM products WHERE id = ?");
        //]

        //[overview_statements_execute
        // Obtain the product_id from the user. product_id is untrusted input
        const char* product_id = get_value_from_user();

        // Execute the statement
        results result;
        conn.execute(stmt.bind(product_id), result);

        // Use result as required
        //]

        conn.execute("DROP TABLE products", result);
    }
    {
        //[overview_errors_sync_errc
        error_code ec;
        diagnostics diag;
        results result;

        // The provided SQL is invalid. The server will return an error.
        // ec will be set to a non-zero value
        conn.execute("this is not SQL!", result, ec, diag);

        if (ec)
        {
            // The error code will likely report a syntax error
            std::cout << "Operation failed with error code: " << ec << '\n';

            // diag.server_message() will contain the classic phrase
            // "You have an error in your SQL syntax; check the manual..."
            // Bear in mind that server_message() may contain user input, so treat it with caution
            std::cout << "Server diagnostics: " << diag.server_message() << std::endl;
        }
        //]
    }
    {
        //[overview_errors_sync_exc
        try
        {
            // The provided SQL is invalid. This function will throw an exception.
            results result;
            conn.execute("this is not SQL!", result);
        }
        catch (const error_with_diagnostics& err)
        {
            // error_with_diagnostics contains an error_code and a diagnostics object.
            // It inherits from boost::system::system_error.
            std::cout << "Operation failed with error code: " << err.code() << '\n'
                      << "Server diagnostics: " << err.get_diagnostics().server_message() << std::endl;
        }
        //]
    }
    {
#ifdef BOOST_ASIO_HAS_CO_AWAIT
        run_coro(conn.get_executor(), [&conn]() -> boost::asio::awaitable<void> {
            //[overview_async_coroutinescpp20
            // Using this CompletionToken, you get C++20 coroutines that communicate
            // errors with error_codes. This way, you can access the diagnostics object.
            constexpr auto token = boost::asio::as_tuple(boost::asio::use_awaitable);

            // Run our query as a coroutine
            diagnostics diag;
            results result;
            auto [ec] = co_await conn.async_execute("SELECT 'Hello world!'", result, diag, token);

            // This will throw an error_with_diagnostics in case of failure
            boost::mysql::throw_on_error(ec, diag);
            //]
        });
#endif
    }
    {
        results r;
        conn.execute("DROP TABLE IF EXISTS posts", r);
    }
    {
        //[overview_multifn
        // Create the table and some sample data
        // In a real system, body may be megabytes long.
        results result;
        conn.execute(
            R"%(
                CREATE TEMPORARY TABLE posts (
                    id INT PRIMARY KEY AUTO_INCREMENT,
                    title VARCHAR (256),
                    body TEXT
                )
            )%",
            result
        );
        conn.execute(
            R"%(
                INSERT INTO posts (title, body) VALUES
                    ('Post 1', 'A very long post body'),
                    ('Post 2', 'An even longer post body')
            )%",
            result
        );

        // execution_state stores state about our operation, and must be passed to all functions
        execution_state st;

        // Writes the query request and reads the server response, but not the rows
        conn.start_execution("SELECT title, body FROM posts", st);

        // Reads all the returned rows, in batches.
        // st.complete() returns true once there are no more rows to read
        while (!st.complete())
        {
            // row_batch will be valid until conn performs the next network operation
            rows_view row_batch = conn.read_some_rows(st);

            for (row_view post : row_batch)
            {
                // Process post as required
                std::cout << "Title:" << post.at(0) << std::endl;
            }
        }
        //]

        conn.execute("DROP TABLE posts", result);
    }
}

}  // namespace
