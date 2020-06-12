//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "boost/mysql/mysql.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/system/system_error.hpp>
#include <boost/asio/use_future.hpp>
#include <iostream>
#include <thread>

using boost::mysql::error_code;
using boost::asio::use_future;

/**
 * For this example, we will be using the 'boost_mysql_examples' database.
 * You can get this database by running db_setup.sql.
 * This example assumes you are connecting to a localhost MySQL server.
 *
 * This example uses asynchronous functions with futures.
 *
 * This example assumes you are already familiar with the basic concepts
 * of mysql-asio (tcp_connection, resultset, rows, values). If you are not,
 * please have a look to the query_sync.cpp example.
 *
 * In this library, all asynchronous operations follow Boost.Asio universal
 * asynchronous models, and thus may be used with callbacks, Boost stackful
 * coroutines, C++20 coroutines or futures.
 * The handler signature is always one of:
 *   - void(error_code): for operations that do not have a "return type" (e.g. handshake)
 *   - void(error_code, T): for operations that have a "return type" (e.g. query, for which
 *     T = resultset<StreamType>).
 *
 * There are two overloads for all asynchronous operations. One accepts an output error_info&
 * parameter right before the completion token. This error_info will be populated
 * in case of error if any extra information provided by the server. The other overload
 * does not have this error_info& parameter.
 *
 * Design note: handler signatures in Boost.Asio should have two parameters, at
 * most, and the first one should be an error_code - otherwise some of the asynchronous
 * features (e.g. coroutines) won't work. This is why error_info is not part of any
 * of the handler signatures.
 */

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

void main_impl(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <username> <password>\n";
        exit(1);
    }

    // Context and connections
    application app; // boost::asio::io_context and a thread that calls run()
    boost::mysql::tcp_connection conn (app.context());

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
     * Perform the TCP connect and MySQL handshake.
     * Calling async_connect triggers the
     * operation, and calling future::get() blocks the current thread until
     * it completes. get() will throw an exception if the operation fails.
     */
    std::future<void> fut = conn.async_connect(ep, params, use_future);
    fut.get();

    // Issue the query to the server
    const char* sql = "SELECT first_name, last_name, salary FROM employee WHERE company_id = 'HGS'";
    std::future<boost::mysql::tcp_resultset> resultset_fut = conn.async_query(sql, use_future);
    boost::mysql::tcp_resultset result = resultset_fut.get();

    /**
     * Get all rows in the resultset. We will employ resultset::async_fetch_one(),
     * which returns a single row at every call. The returned row is a pointer
     * to memory owned by the resultset, and is re-used for each row. Thus, returned
     * rows remain valid until the next call to async_fetch_one(). When no more
     * rows are available, async_fetch_one returns nullptr.
     */
    while (const boost::mysql::row* current_row = result.async_fetch_one(use_future).get())
    {
        print_employee(*current_row);
    }

    // Notify the MySQL server we want to quit, then close the underlying connection.
    conn.async_close(use_future).get();

    // application dtor. stops io_context and then joins the thread
}

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
