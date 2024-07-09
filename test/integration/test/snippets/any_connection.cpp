//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <thread>

#include "test_common/ci_server.hpp"
#include "test_integration/snippets/credentials.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;

namespace {

//[any_connection_tcp
void create_and_connect(
    string_view server_hostname,
    string_view username,
    string_view password,
    string_view database
)
{
    // connect_params contains all the info required to establish a session
    boost::mysql::connect_params params;
    params.server_address.emplace_host_and_port(server_hostname);  // server host
    params.username = username;                                    // username to log in as
    params.password = password;                                    // password to use
    params.database = database;                                    // database to use

    // The execution context, required to run I/O operations.
    boost::asio::io_context ctx;

    // A connection to the server. Note how the type doesn't depend
    // on the transport being used.
    boost::mysql::any_connection conn(ctx);

    // Connect to the server. This will perform hostname resolution,
    // TCP-level connect, and the MySQL handshake. After this function
    // succeeds, your connection is ready to run queries
    conn.connect(params);
}
//]

BOOST_AUTO_TEST_CASE(section_any_connection_tcp)
{
    create_and_connect(get_hostname(), mysql_username, mysql_password, "boost_mysql_examples");
}

// Intentionally not run, since it creates problems in Windows CIs
//[any_connection_unix
void create_and_connect_unix(string_view username, string_view password, string_view database)
{
    // server_address may contain a UNIX socket path, too
    boost::mysql::connect_params params;
    params.server_address.emplace_unix_path("/var/run/mysqld/mysqld.sock");
    params.username = username;  // username to log in as
    params.password = password;  // password to use
    params.database = database;  // database to use

    // The execution context, required to run I/O operations.
    boost::asio::io_context ctx;

    // A connection to the server. Note how the type doesn't depend
    // on the transport being used.
    boost::mysql::any_connection conn(ctx);

    // Connect to the server. This will perform the
    // UNIX socket connect and the MySQL handshake. After this function
    // succeeds, your connection is ready to run queries
    conn.connect(params);
}
//]

BOOST_TEST_DECORATOR(*boost::unit_test::label("unix"))
BOOST_AUTO_TEST_CASE(section_any_connection_unix)
{
    create_and_connect_unix(mysql_username, mysql_password, "boost_mysql_examples");
}

//[any_connection_reconnect
error_code connect_with_retries(
    boost::mysql::any_connection& conn,
    const boost::mysql::connect_params& params
)
{
    // We will be using the non-throwing overloads
    error_code ec;
    diagnostics diag;

    // Try to connect at most 10 times
    for (int i = 0; i < 10; ++i)
    {
        // Try to connect
        conn.connect(params, ec, diag);

        // If we succeeded, we're done
        if (!ec)
            return error_code();

        // Whoops, connect failed. We can sleep and try again
        std::cerr << "Failed connecting to MySQL: " << ec << ": " << diag.server_message() << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // No luck, retries expired
    return ec;
}
//]

BOOST_AUTO_TEST_CASE(section_any_connection)
{
    auto server_hostname = get_hostname();

    {
        connect_params params;
        params.server_address.emplace_host_and_port(server_hostname);
        params.username = mysql_username;
        params.password = mysql_password;

        boost::asio::io_context ctx;
        any_connection conn(ctx);
        auto ec = connect_with_retries(conn, params);
        BOOST_TEST(ec == error_code());
    }

    {
        connect_params params;

        //[any_connection_ssl_mode
        // Don't ever use TLS, even if the server supports it
        params.ssl = boost::mysql::ssl_mode::disable;

        // ...

        // Force using TLS. If the server doesn't support it, reject the connection
        params.ssl = boost::mysql::ssl_mode::require;
        //]
    }

    {
        //[any_connection_ssl_ctx
        // The I/O context required to run network operations
        boost::asio::io_context ctx;

        // Create a SSL context
        boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::tlsv12_client);

        // Set options on the SSL context. Load the default certificate authorities
        // and enable certificate verification. connect will fail if the server certificate
        // isn't signed by a trusted entity or its hostname isn't "mysql"
        ssl_ctx.set_default_verify_paths();
        ssl_ctx.set_verify_mode(boost::asio::ssl::verify_peer);
        ssl_ctx.set_verify_callback(boost::asio::ssl::host_name_verification("mysql"));

        // Construct an any_connection object passing the SSL context.
        // You must keep ssl_ctx alive while using the connection.
        boost::mysql::any_connection_params ctor_params;
        ctor_params.ssl_context = &ssl_ctx;
        boost::mysql::any_connection conn(ctx, ctor_params);

        // Connect params
        boost::mysql::connect_params params;
        params.server_address.emplace_host_and_port(server_hostname);  // server host
        params.username = mysql_username;                              // username to log in as
        params.password = mysql_password;                              // password to use
        params.ssl = boost::mysql::ssl_mode::require;                  // fail if TLS is not available

        // Connect
        error_code ec;
        diagnostics diag;
        conn.connect(params, ec, diag);
        if (ec)
        {
            // Handle error
        }
        //]
        BOOST_TEST(ec != error_code());
        BOOST_TEST((ec.category() == boost::asio::error::get_ssl_category()));
    }
}

}  // namespace
