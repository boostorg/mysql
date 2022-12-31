//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//[example_timeouts

#include <boost/mysql.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/system/system_error.hpp>

#include <chrono>
#include <exception>
#include <iostream>
#include <stdexcept>

#ifdef BOOST_ASIO_HAS_CO_AWAIT

#include <boost/asio/experimental/awaitable_operators.hpp>

using namespace boost::asio::experimental::awaitable_operators;
using boost::asio::use_awaitable;
using boost::mysql::error_code;

constexpr std::chrono::milliseconds TIMEOUT(2000);

void print_employee(boost::mysql::row_view employee)
{
    std::cout << "Employee '" << employee.at(0) << " "   // first_name (string)
              << employee.at(1) << "' earns "            // last_name  (string)
              << employee.at(2) << " dollars yearly\n";  // salary     (double)
}

/**
 * Helper functions to check whether an async operation, launched in parallel with
 * a timer, was successful or instead timed out. The timer is always the first operation.
 * If the variant holds the first altrnative, that means that the timer fired before
 * the async operation completed, which means a timeout.
 */
template <class T>
T check_timeout(std::variant<std::monostate, T>&& op_result)
{
    if (op_result.index() == 0)
    {
        throw std::runtime_error("Operation timed out");
    }
    return std::get<1>(std::move(op_result));
}

/**
 * We use Boost.Asio's cancellation capabilities to implement timeouts for our
 * asynchronous operations. This is not something specific to Boost.Mysql, and
 * can be used with any other asynchronous operation that follows Asio's model.
 *
 * Each time we invoke an asynchronous operation, we also call steady_timer::async_wait.
 * We then use Asio's overload for operator || to run the timer wait and the async operation
 * in parallel. Once the first of them finishes, the other operation is cancelled
 * (the behavior is similar to JavaScripts's Promise.race).
 * If we co_await the awaitable returned by operator ||, we get a std::variant<std::monostate, T>,
 * where T is the async operation's result type. If the timer wait finishes first (we have a
 * timeout), the variant will hold the std::monostate at index 0; otherwise, it will have the async
 * operation's result at index 1. The function check_timeout throws an exception in the case of
 * timeout and extracts the operation's result otherwise.
 *
 * If any of the MySQL specific operations result in a timeout, the connection is left
 * in an unspecified state. You should close it and re-open it to get it working again.
 */
boost::asio::awaitable<void> start_query(
    boost::mysql::tcp_ssl_connection& conn,
    boost::asio::ip::tcp::resolver& resolver,
    boost::asio::steady_timer& timer,
    const boost::mysql::handshake_params& params,
    const char* hostname,
    const char* company_id
)
{
    try
    {
        // Resolve hostname
        timer.expires_after(TIMEOUT);
        auto endpoints = check_timeout(co_await (
            timer.async_wait(use_awaitable) ||
            resolver.async_resolve(hostname, boost::mysql::default_port_string, use_awaitable)
        ));

        // Connect to server. Note that we need to reset the timer before using it again.
        timer.expires_after(TIMEOUT);
        check_timeout(co_await (
            timer.async_wait(use_awaitable) ||
            conn.async_connect(*endpoints.begin(), params, use_awaitable)
        ));

        // We will be using company_id, which is untrusted user input, so we will use a prepared
        // statement.
        boost::mysql::tcp_ssl_statement stmt;
        check_timeout(co_await (
            timer.async_wait(use_awaitable) ||
            conn.async_prepare_statement(
                "SELECT first_name, last_name, salary FROM employee WHERE company_id = ?",
                stmt,
                use_awaitable
            )
        ));

        // Execute the statement
        boost::mysql::resultset result;
        timer.expires_after(TIMEOUT);
        check_timeout(co_await (
            timer.async_wait(use_awaitable) ||
            stmt.async_execute(std::make_tuple(company_id), result, use_awaitable)
        ));

        // Print all the obtained rows
        for (boost::mysql::row_view employee : result.rows())
        {
            print_employee(employee);
        }

        // Notify the MySQL server we want to quit, then close the underlying connection.
        check_timeout(co_await (timer.async_wait(use_awaitable) || conn.async_close(use_awaitable))
        );
    }
    catch (const boost::system::system_error& err)
    {
        std::cerr << "Error: " << err.what() << ", error code: " << err.code() << std::endl;
    }
    catch (const std::exception& err)
    {
        std::cerr << "Error: " << err.what() << std::endl;
    }
}

void main_impl(int argc, char** argv)
{
    if (argc != 4 && argc != 5)
    {
        std::cerr << "Usage: " << argv[0]
                  << " <username> <password> <server-hostname> [company-id]\n";
        exit(1);
    }

    const char* hostname = argv[3];

    // The company_id whose employees we will be listing. This
    // is user-supplied input, and should be treated as untrusted.
    const char* company_id = argc == 5 ? argv[4] : "HGS";

    // I/O context and connection. We use SSL because MySQL 8+ default settings require it.
    boost::asio::io_context ctx;
    boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::tls_client);
    boost::mysql::tcp_ssl_connection conn(ctx, ssl_ctx);
    boost::asio::steady_timer timer(ctx.get_executor());

    // Connection parameters
    boost::mysql::handshake_params params(
        argv[1],                // username
        argv[2],                // password
        "boost_mysql_examples"  // database to use; leave empty or omit for no database
    );

    // Resolver for hostname resolution
    boost::asio::ip::tcp::resolver resolver(ctx.get_executor());

    // The entry point. We pass in a function returning a boost::asio::awaitable<void>, as required.
    boost::asio::co_spawn(
        ctx.get_executor(),
        [&conn, &resolver, &timer, params, hostname, company_id] {
            return start_query(conn, resolver, timer, params, hostname, company_id);
        },
        boost::asio::detached
    );

    // Calling run will actually start the requested operations.
    ctx.run();
}

#else

void main_impl(int, char**)
{
    std::cout << "Sorry, your compiler does not support C++20 coroutines" << std::endl;
}

#endif

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
