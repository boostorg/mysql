//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//[example_default_completion_tokens]
#include "boost/mysql/mysql.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/system/system_error.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_future.hpp>
#include <iostream>

using boost::mysql::error_code;
using boost::mysql::error_info;


#ifdef BOOST_ASIO_HAS_CO_AWAIT

void print_employee(const boost::mysql::row& employee)
{
    std::cout << "Employee '"
              << employee.values()[0] << " "                   // first_name (type boost::string_view)
              << employee.values()[1] << "' earns "            // last_name  (type boost::string_view)
              << employee.values()[2] << " dollars yearly\n";  // salary     (type double)
}

/**
 * A boost::asio::io_context plus a thread that calls context.run().
 * We encapsulate this here to ensure correct shutdown even in case of
 * error (exception), when we should first reset the work guard, to
 * stop the io_context, and then join the thread. Failing to do so
 * may cause your application to not stop (if the work guard is not
 * reset) or to terminate badly (if the thread is not joined).
 */
class application
{
    boost::asio::io_context ctx_;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> guard_;
    std::thread runner_;
public:
    application(): guard_(ctx_.get_executor()), runner_([this] { ctx_.run(); }) {}
    application(const application&) = delete;
    application(application&&) = delete;
    application& operator=(const application&) = delete;
    application& operator=(application&&) = delete;
    ~application()
    {
        guard_.reset();
        runner_.join();
    }
    boost::asio::io_context& context() { return ctx_; }
};

/**
 * Default completion tokens are associated to executors.
 * boost::mysql::socket_connection objects use the same executor
 * as the underlying stream (socket). boost::mysql::tcp_connection
 * objects use boost::asio::ip::tcp::socket, which use the polymorphic
 * boost::asio::executor as executor type, which does not have a default
 * completion token associated.
 *
 * We will use the io_context's executor as base executor. We will then
 * use use_awaitable_t::executor_with_default on this type, which creates
 * a new executor type acting the same as the base executor, but having
 * use_awaitable_t as default completion token type.
 *
 * We will then obtain the connection type to use by rebinding
 * the usual tcp_connection to our new executor type, coro_executor_type.
 * This is equivalent to using a boost::mysql::connection<socket_type>,
 * where socket_type is a TCP socket that uses our coro_executor_type.
 *
 * The reward for this hard work is not having to pass the completion
 * token (boost::asio::use_awaitable) to any of the asynchronous operations
 * initiated by this connection or any of the I/O objects (e.g. resultsets)
 * associated to them.
 */
using base_executor_type = boost::asio::io_context::executor_type;
using coro_executor_type = boost::asio::use_awaitable_t<
    base_executor_type>::executor_with_default<base_executor_type>;
using connection_type = boost::mysql::tcp_connection::rebind_executor<coro_executor_type>::other;

// Our coroutine
boost::asio::awaitable<void, base_executor_type> start_query(
    const boost::asio::io_context::executor_type& ex,
    const boost::asio::ip::tcp::endpoint& ep,
    const boost::mysql::connection_params& params
)
{
    // Create the connection. We do not use the raw tcp_connection type
    // alias to default the completion token; see above.
    connection_type conn (ex);

    // Connect to server. Note: we didn't have to pass boost::asio::use_awaitable.
    co_await conn.async_connect(ep, params);

    /**
     * Issue the query to the server. Note that async_query returns a
     * boost::asio::awaitable<boost::mysql::resultset<socket_type>, base_executor_type>,
     * where socket_type is a TCP socket bound to coro_executor_type.
     * Calling co_await on this expression will yield a boost::mysql::resultset<socket_type>.
     * Note that this is not the same type as a boost::mysql::tcp_resultset because we
     * used a custom socket type.
     */
    const char* sql = "SELECT first_name, last_name, salary FROM employee WHERE company_id = 'HGS'";
    auto result = co_await conn.async_query(sql);

    /**
     * Get all rows in the resultset. We will employ resultset::async_fetch_one(),
     * which returns a single row at every call. The returned row is a pointer
     * to memory owned by the resultset, and is re-used for each row. Thus, returned
     * rows remain valid until the next call to async_fetch_one(). When no more
     * rows are available, async_fetch_one returns nullptr.
     */
    while (const boost::mysql::row* row = co_await result.async_fetch_one())
    {
        print_employee(*row);
    }

    // Notify the MySQL server we want to quit, then close the underlying connection.
    co_await conn.async_close();
}

void main_impl(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <username> <password>\n";
        exit(1);
    }

    // io_context plus runner thread
    application app;

    // Connection parameters
    boost::asio::ip::tcp::endpoint ep (
        boost::asio::ip::address_v4::loopback(), // host
        boost::mysql::default_port                 // port
    );
    boost::mysql::connection_params params (
        argv[1],               // username
        argv[2],               // password
        "boost_mysql_examples" // database to use; leave empty or omit the parameter for no database
    );

    /**
     * The entry point. We spawn a thread of execution to run our
     * coroutine using boost::asio::co_spawn. We pass in a function returning
     * a boost::asio::awaitable<void>, as required.
     *
     * We pass in a callback to co_spawn. It will be called when
     * the coroutine completes, with an exception_ptr if there was any error
     * during execution. We use a promise to wait for the coroutine completion
     * and transmit any raised exception.
     */
    auto executor = app.context().get_executor();
    std::promise<void> prom;
    boost::asio::co_spawn(executor, [executor, ep, params] {
        return start_query(executor, ep, params);
    }, [&prom](std::exception_ptr err) {
        prom.set_exception(std::move(err));
    });
    prom.get_future().get();
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
    catch (const boost::system::system_error& err)
    {
        std::cerr << "Error: " << err.what() << ", error code: " << err.code() << std::endl;
        return 1;
    }
    catch (const std::exception& err)
    {
        std::cerr << "Error: " << err.what() << std::endl;
        return 1;
    }
}

//]

