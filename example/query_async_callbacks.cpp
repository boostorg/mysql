//
// Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//[example_query_async_callbacks

#include "boost/mysql/mysql.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/system/system_error.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/yield.hpp>
#include <iostream>

using boost::mysql::error_code;
using boost::mysql::error_info;
using boost::mysql::tcp_resultset;
using boost::mysql::row;

#define ASSERT(expr) \
    if (!(expr)) \
    { \
        std::cerr << "Assertion failed: " #expr << std::endl; \
        exit(1); \
    }

void print_employee(const boost::mysql::row& employee)
{
    std::cout << "Employee '"
              << employee.values()[0] << " "                   // first_name (type boost::string_view)
              << employee.values()[1] << "' earns "            // last_name  (type boost::string_view)
              << employee.values()[2] << " dollars yearly\n";  // salary     (type double)
}

void die_on_error(
    const error_code& err,
    const boost::mysql::error_info& info = boost::mysql::error_info()
)
{
    if (err)
    {
        std::cerr << "Error: " << err << ": " << info.message() << std::endl;
        exit(1);
    }
}

class application
{
    boost::asio::ip::tcp::endpoint ep;            // Physical endpoint to connect to
    boost::mysql::connection_params conn_params;  // MySQL credentials and other connection config
    boost::asio::io_context ctx;                  // boost::asio context
    boost::mysql::tcp_connection connection;      // Represents the connection to the MySQL server
    boost::mysql::tcp_resultset resultset;        // A result from a query
    boost::mysql::error_info additional_info;     // Will be populated with additional information about any errors
public:
    application(const char* username, const char* password) :
        ep (boost::asio::ip::address_v4::loopback(), boost::mysql::default_port),
        conn_params(username, password, "boost_mysql_examples"),
        connection(ctx)
    {
    }

    void start() { connect(); }

    void connect()
    {
        connection.async_connect(ep, conn_params, additional_info, [this](error_code err) {
            die_on_error(err, additional_info);
            query_employees();
        });
    }

    void query_employees()
    {
        const char* sql = "SELECT first_name, last_name, salary FROM employee WHERE company_id = 'HGS'";
        connection.async_query(sql, additional_info, [this](error_code err, tcp_resultset&& result) {
            die_on_error(err, additional_info);
            resultset = std::move(result);
            resultset.async_read_all(additional_info, [this](error_code err, const std::vector<row>& rows) {
                die_on_error(err, additional_info);
                for (const auto& employee: rows)
                {
                    print_employee(employee);
                }
                update_slacker();
            });
        });
    }

    void update_slacker()
    {
        const char* sql = "UPDATE employee SET salary = 15000 WHERE last_name = 'Slacker'";
        connection.async_query(sql, additional_info,
                [this](error_code err, tcp_resultset&& result) {
            die_on_error(err, additional_info);
            ASSERT(result.fields().size() == 0);
            query_intern();
        });
    }

    void query_intern()
    {
        const char* sql = "SELECT salary FROM employee WHERE last_name = 'Slacker'";
        connection.async_query(sql, additional_info, [this](error_code err, tcp_resultset&& result) {
            die_on_error(err, additional_info);
            resultset = std::move(result);
            resultset.async_read_all(additional_info, [this](error_code err, const std::vector<row>& rows) {
                die_on_error(err, additional_info);
                ASSERT(rows.size() == 1);
                auto salary = rows[0].values()[0].get<double>();
                ASSERT(salary == 15000);
                close();
            });
        });
    }

    void close()
    {
        // Notify the MySQL server we want to quit and then close the socket
        connection.async_close(additional_info, [this](error_code err) {
            die_on_error(err, additional_info);
        });
    }

    void run() { ctx.run(); }
};

void main_impl(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <username> <password>\n";
        exit(1);
    }

    application app (argv[1], argv[2]);
    app.start(); // starts the async chain
    app.run(); // run the asio::io_context until the async chain finishes
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
