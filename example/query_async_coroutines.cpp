//
// Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//[example_query_async_coroutines

#include <boost/mysql.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/system/system_error.hpp>
#include <boost/asio/spawn.hpp>
#include <iostream>

using boost::mysql::error_code;
using boost::mysql::error_info;

void print_employee(const boost::mysql::row& employee)
{
    std::cout << "Employee '"
              << employee.values()[0] << " "                   // first_name (type boost::string_view)
              << employee.values()[1] << "' earns "            // last_name  (type boost::string_view)
              << employee.values()[2] << " dollars yearly\n";  // salary     (type double)
}

// Throws an exception if an operation failed
void check_error(
    const error_code& err,
    const error_info& info = {}
)
{
    if (err)
    {
        throw boost::system::system_error(err, info.message());
    }
}


void main_impl(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <username> <password>\n";
        exit(1);
    }

    // Context and connections
    boost::asio::io_context ctx;
    boost::mysql::tcp_connection conn (ctx);

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
     * The entry point. We spawn a stackful coroutine using boost::asio::spawn
     * (see https://www.boost.org/doc/libs/1_72_0/doc/html/boost_asio/reference/spawn.html).
     *
     * The coroutine will actually start running when we call io_context::run().
     * It will suspend every time we call one of the asynchronous functions, saving
     * all information it needs for resuming. When the asynchronous operation completes,
     * the coroutine will resume in the point it was left.
     *
     * The return type of a coroutine is the second argument to the handler signature
     * for the asynchronous operation. For example, connection::query has a handler
     * signature of void(error_code, resultset<Stream>), so the coroutine return
     * type is resultset<Stream>.
     *
     */
    boost::asio::spawn(ctx.get_executor(), [&conn, ep, params](boost::asio::yield_context yield) {
        // This error_code and error_info will be filled if an
        // operation fails. We will check them for every operation we perform.
        boost::mysql::error_code ec;
        boost::mysql::error_info additional_info;

        // Connect to server
        conn.async_connect(ep, params, additional_info, yield[ec]);
        check_error(ec);

        // Issue the query to the server
        const char* sql = "SELECT first_name, last_name, salary FROM employee WHERE company_id = 'HGS'";
        boost::mysql::tcp_resultset result = conn.async_query(sql, additional_info, yield[ec]);
        check_error(ec, additional_info);

        /**
          * Get all rows in the resultset. We will employ resultset::async_read_one(),
          * which reads a single row at every call. The row is read in-place, preventing
          * unnecessary copies. resultset::async_read_one() returns true if a row has been
          * read, false if no more rows are available or an error occurred.
         */
        boost::mysql::row row;
        while (result.async_read_one(row, additional_info, yield[ec]))
        {
            check_error(ec, additional_info);
            print_employee(row);
        }

        // Notify the MySQL server we want to quit, then close the underlying connection.
        conn.async_close(additional_info, yield[ec]);
        check_error(ec, additional_info);
    });

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
