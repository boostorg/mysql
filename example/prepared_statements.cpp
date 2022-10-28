//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//[example_prepared_statements

#include <boost/mysql.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/handshake_params.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/system/system_error.hpp>

#include <iostream>
#include <tuple>

#define ASSERT(expr)                                          \
    if (!(expr))                                              \
    {                                                         \
        std::cerr << "Assertion failed: " #expr << std::endl; \
        exit(1);                                              \
    }

void main_impl(int argc, char** argv)
{
    if (argc != 4)
    {
        std::cerr << "Usage: " << argv[0] << " <username> <password> <server-hostname>\n";
        exit(1);
    }

    // I/O context and connection. We use SSL because MySQL 8+ default settings require it.
    boost::asio::io_context ctx;
    boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::tls_client);
    boost::mysql::tcp_ssl_connection conn(ctx, ssl_ctx);

    // Resolver for hostname resolution
    boost::asio::ip::tcp::resolver resolver(ctx.get_executor());

    // Connection params
    boost::mysql::handshake_params params(
        argv[1],                // username
        argv[2],                // password
        "boost_mysql_examples"  // database to use; leave empty or omit the parameter for no
                                // database
    );

    // Hostname resolution
    auto endpoints = resolver.resolve(argv[3], boost::mysql::default_port_string);

    // TCP and MySQL level connect
    conn.connect(*endpoints.begin(), params);

    /**
     * We can tell MySQL to prepare a statement using connection::prepare_statement.
     * We provide a string SQL statement, which can include any number of parameters,
     * identified by question marks. Parameters are optional: you can prepare a statement
     * with no parameters.
     *
     * Prepared statements are stored in the server on a per-connection basis.
     * Once a connection is closed, all prepared statements for that connection are deallocated.
     *
     * The result of prepare_statement is a boost::mysql::statement object, which is
     * templatized on the stream type of the connection (tcp_ssl_statement in our case).
     *
     * We prepare two statements, a SELECT and an UPDATE.
     */
    //[prepared_statements_prepare
    const char* salary_getter_sql = "SELECT salary FROM employee WHERE first_name = ?";
    boost::mysql::tcp_ssl_statement salary_getter;
    conn.prepare_statement(salary_getter_sql, salary_getter);
    //]

    // num_params() returns the number of parameters (question marks)
    ASSERT(salary_getter.num_params() == 1);

    const char* salary_updater_sql = "UPDATE employee SET salary = ? WHERE first_name = ?";
    boost::mysql::tcp_ssl_statement salary_updater;
    conn.prepare_statement(salary_updater_sql, salary_updater);
    ASSERT(salary_updater.num_params() == 2);

    // TODO: update this
    /*
     * Once a statement has been prepared, it can be executed as many times as
     * desired, by calling statement::execute(). execute takes as input a
     * (possibly empty) collection of boost::mysql::value's and returns a resultset (by lvalue
     * reference). The returned resultset works the same as the one returned by connection::query().
     *
     * The parameters passed to execute() are replaced in the order of declaration:
     * the first question mark will be replaced by the first passed parameter,
     * the second question mark by the second parameter and so on. The number
     * of passed parameters must match exactly the number of parameters for
     * the prepared statement.
     *
     * Any collection providing member functions begin() and end() returning
     * forward iterators to boost::mysql::field_view's is acceptable. We use
     * boost::mysql::make_field_views(), which creates a std::array with the passed in values
     * converted to boost::mysql::field_view's.
     */
    //[prepared_statements_execute
    boost::mysql::tcp_ssl_resultset result;
    salary_getter.execute(std::make_tuple("Efficient"), result);

    boost::mysql::rows salaries;
    result.read_all(salaries);  // Get all the results
    //]
    ASSERT(salaries.size() == 1);
    double salary = salaries[0].at(0).as_double();  // First row, first column, cast to double
    std::cout << "The salary before the payrise was: " << salary << std::endl;

    // Run the update. In this case, we must pass in two parameters.
    salary_updater.execute(std::make_tuple(35000.0, "Efficient"), result);
    ASSERT(result.complete());  // an UPDATE never returns rows

    /**
     * Execute the select again. We can execute a prepared statement
     * as many times as we want. We do NOT need to call
     * connection::prepare_statement() again.
     */
    salary_getter.execute(std::make_tuple("Efficient"), result);
    result.read_all(salaries);
    salary = salaries.at(0).at(0).as_double();
    ASSERT(salary == 35000);  // Our update took place, and the dev got his pay rise
    std::cout << "The salary after the payrise was: " << salary << std::endl;

    /**
     * Close the statements. Closing a statement deallocates it from the server.
     * Once a statement is closed, trying to execute it will return an error.
     *
     * Closing statements implies communicating with the server and can thus fail.
     *
     * Statements are automatically deallocated once the connection is closed.
     * If you are re-using connection objects and preparing statements over time,
     * you should close() your statements to prevent excessive resource usage.
     * If you are not re-using the connections, or are preparing your statements
     * just once at application startup, there is no need to perform this step.
     */
    salary_updater.close();
    salary_getter.close();

    // Close the connection
    conn.close();
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
