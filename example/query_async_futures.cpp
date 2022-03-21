//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//[example_query_async_futures

#include <boost/asio/ssl/context.hpp>
#include <boost/mysql.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/system/system_error.hpp>
#include <boost/asio/use_future.hpp>
#include <iostream>
#include <thread>

using boost::mysql::error_code;
using boost::asio::use_future;

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
    if (argc != 4)
    {
        std::cerr << "Usage: " << argv[0] << " <username> <password> <server-hostname>\n";
        exit(1);
    }

    // Context and connections
    application app; // boost::asio::io_context and a thread that calls run()
    boost::asio::ssl::context ssl_ctx (boost::asio::ssl::context::tls_client);
    boost::mysql::tcp_ssl_connection conn (app.context(), ssl_ctx);

    // Resolver for hostname resolution
    boost::asio::ip::tcp::resolver resolver (app.context().get_executor());

    // Connection params
    boost::mysql::connection_params params (
        argv[1],               // username
        argv[2],               // password
        "boost_mysql_examples" // database to use; leave empty or omit the parameter for no database
    );

   /**
     * Hostname resolution.
     * Calling async_resolve triggers the
     * operation, and calling future::get() blocks the current thread until
     * it completes. get() will throw an exception if the operation fails.
     */
    auto endpoints = resolver.async_resolve(
        argv[3],
        boost::mysql::default_port_string,
        boost::asio::use_future
    ).get();


    // Perform the TCP connect and MySQL handshake.
    std::future<void> fut = conn.async_connect(*endpoints.begin(), params, use_future);
    fut.get();

    // Issue the query to the server
    const char* sql = "SELECT first_name, last_name, salary FROM employee WHERE company_id = 'HGS'";
    std::future<boost::mysql::tcp_ssl_resultset> resultset_fut = conn.async_query(sql, use_future);
    boost::mysql::tcp_ssl_resultset result = resultset_fut.get();

    /**
      * Get all rows in the resultset. We will employ resultset::async_read_one(),
      * which reads a single row at every call. The row is read in-place, preventing
      * unnecessary copies. resultset::async_read_one() returns true if a row has been
      * read, false if no more rows are available or an error occurred.
      */
    boost::mysql::row row;
    while (result.async_read_one(row, use_future).get())
    {
        print_employee(row);
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

//]
