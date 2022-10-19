//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//[example_query_async_callbacks

#include <boost/mysql.hpp>
#include <boost/mysql/connection.hpp>
#include <boost/mysql/error.hpp>
#include <boost/mysql/handshake_params.hpp>

#include <boost/asio/coroutine.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/yield.hpp>
#include <boost/system/system_error.hpp>

#include <iostream>

using boost::mysql::error_code;

#define ASSERT(expr)                                          \
    if (!(expr))                                              \
    {                                                         \
        std::cerr << "Assertion failed: " #expr << std::endl; \
        exit(1);                                              \
    }

void print_employee(boost::mysql::row_view employee)
{
    std::cout << "Employee '" << employee[0] << " "   // first_name (string)
              << employee[1] << "' earns "            // last_name  (string)
              << employee[2] << " dollars yearly\n";  // salary     (double)
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
    boost::asio::ip::tcp::resolver::results_type eps;  // Physical endpoint(s) to connect to
    boost::mysql::handshake_params conn_params;  // MySQL credentials and other connection config
    boost::asio::io_context ctx;                 // boost::asio context
    boost::asio::ip::tcp::resolver resolver;     // To perform hostname resolution
    boost::asio::ssl::context ssl_ctx;           // MySQL 8+ default settings require SSL
    boost::mysql::tcp_ssl_connection conn;       // Represents the connection to the MySQL server
    boost::mysql::tcp_ssl_resultset resultset;   // A result from a query
    boost::mysql::rows rows;                     // The rows to be read from the resultset
    boost::mysql::error_info
        additional_info;  // Will be populated with additional information about any errors
public:
    application(const char* username, const char* password)
        : conn_params(username, password, "boost_mysql_examples"),
          resolver(ctx.get_executor()),
          ssl_ctx(boost::asio::ssl::context::tls_client),
          conn(ctx, ssl_ctx)
    {
    }

    void start(const char* hostname) { resolve_hostname(hostname); }

    void resolve_hostname(const char* hostname)
    {
        resolver.async_resolve(
            hostname,
            boost::mysql::default_port_string,
            [this](error_code err, boost::asio::ip::tcp::resolver::results_type results) {
                die_on_error(err);
                eps = std::move(results);
                connect();
            }
        );
    }

    void connect()
    {
        conn.async_connect(*eps.begin(), conn_params, additional_info, [this](error_code err) {
            die_on_error(err, additional_info);
            query_employees();
        });
    }

    void query_employees()
    {
        const char*
            sql = "SELECT first_name, last_name, salary FROM employee WHERE company_id = 'HGS'";
        conn.async_query(sql, resultset, additional_info, [this](error_code err) {
            die_on_error(err, additional_info);
            resultset.async_read_all(rows, additional_info, [this](error_code err) {
                die_on_error(err, additional_info);
                for (const auto& employee : rows)
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
        conn.async_query(sql, resultset, additional_info, [this](error_code err) {
            die_on_error(err, additional_info);
            ASSERT(resultset.complete());  // an UPDATE never returns rows
            query_intern();
        });
    }

    void query_intern()
    {
        const char* sql = "SELECT salary FROM employee WHERE last_name = 'Slacker'";
        conn.async_query(sql, resultset, additional_info, [this](error_code err) {
            die_on_error(err, additional_info);
            resultset.async_read_all(rows, additional_info, [this](error_code err) {
                die_on_error(err, additional_info);
                double salary = rows.at(0).at(0).as_double();
                ASSERT(salary == 15000);
                close();
            });
        });
    }

    void close()
    {
        // Notify the MySQL server we want to quit and then close the socket
        conn.async_close(additional_info, [this](error_code err) {
            die_on_error(err, additional_info);
        });
    }

    void run() { ctx.run(); }
};

void main_impl(int argc, char** argv)
{
    if (argc != 4)
    {
        std::cerr << "Usage: " << argv[0] << " <username> <password> <server-hostname>\n";
        exit(1);
    }

    application app(argv[1], argv[2]);
    app.start(argv[3]);  // starts the async chain
    app.run();           // run the asio::io_context until the async chain finishes
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
