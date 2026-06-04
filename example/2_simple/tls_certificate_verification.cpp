//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/awaitable.hpp>
#ifdef BOOST_ASIO_HAS_CO_AWAIT

//[example_tls_certificate_verification

/**
 * This example demonstrates how to set up TLS certificate verification
 * and, more generally, how to pass custom TLS options to any_connection.
 *
 * It uses C++20 coroutines. If you need, you can backport
 * it to C++11 by using callbacks, asio::yield_context
 * or sync functions instead of coroutines.
 *
 * This example uses the 'boost_mysql_examples' database, which you
 * can get by running db_setup.sql.
 * Additionally, your server must be configured with a trusted certificate
 * with a common name of "mysql".
 */

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/results.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/asio/this_coro.hpp>

#include <iostream>

namespace mysql = boost::mysql;
namespace asio = boost::asio;

// The main coroutine
asio::awaitable<void> coro_main(
    std::string_view server_hostname,
    std::string_view username,
    std::string_view password,
    std::string_view ca_cert_path
)
{
    //[section_connection_establishment_tls_options
    // Create a SSL context, which contains TLS configuration options
    asio::ssl::context ssl_ctx(asio::ssl::context::tls_client);

    // Enable certificate verification. If the server's certificate
    // is not valid or not signed by a trusted CA, async_connect will error.
    ssl_ctx.set_verify_mode(asio::ssl::verify_peer);

    // Load a trusted CA, which was used to sign the server's certificate.
    // This will allow the signature verification to succeed in our example.
    // If you want to use your system's trusted CAs, use
    // ssl::context::set_default_verify_paths() instead of this function.
    ssl_ctx.load_verify_file(std::string(ca_cert_path));

    // We expect the server certificate's common name to be "mysql".
    // If it's not, the certificate will be rejected and handshake or connect will fail.
    // Replace "mysql" by the common name you expect.
    ssl_ctx.set_verify_callback(asio::ssl::host_name_verification("mysql"));

    // Create a connection.
    // We pass the context as the second argument to the connection's constructor.
    // Other TLS options can be also configured using this approach.
    // We need to keep ssl_ctx alive as long as we use the connection.
    mysql::any_connection conn(co_await asio::this_coro::executor, mysql::any_connection_params{&ssl_ctx});

    // The hostname, username, password and database to use
    mysql::connect_params params;
    params.server_address.emplace_host_and_port(std::string(server_hostname));
    params.username = username;
    params.password = password;
    params.database = "boost_mysql_examples";

    // Connect to the server. If certificate verification fails,
    // async_connect will fail.
    co_await conn.async_connect(params);
    //]

    // The connection can now be used normally
    mysql::results result;
    co_await conn.async_execute("SELECT 'Hello world!'", result);
    std::cout << result.rows().at(0).at(0) << std::endl;

    // Notify the MySQL server we want to quit, then close the underlying connection.
    co_await conn.async_close();
}

void main_impl(int argc, char** argv)
{
    if (argc != 5)
    {
        std::cerr << "Usage: " << argv[0] << " <username> <password> <server-hostname> <ca-cert-path>\n";
        exit(1);
    }

    // Create an I/O context, required by all I/O objects
    asio::io_context ctx;

    // Launch our coroutine
    asio::co_spawn(
        ctx,
        [=] { return coro_main(argv[3], argv[1], argv[2], argv[4]); },
        // If any exception is thrown in the coroutine body, rethrow it.
        [](std::exception_ptr ptr) {
            if (ptr)
            {
                std::rethrow_exception(ptr);
            }
        }
    );

    // Calling run will actually execute the coroutine until completion
    ctx.run();

    std::cout << "Done\n";
}

int main(int argc, char** argv)
{
    try
    {
        main_impl(argc, argv);
    }
    catch (const boost::mysql::error_with_diagnostics& err)
    {
        // Some errors include additional diagnostics, like server-provided error messages.
        // Security note: diagnostics::server_message may contain user-supplied values (e.g. the
        // field value that caused the error) and is encoded using to the connection's character set
        // (UTF-8 by default). Treat is as untrusted input.
        std::cerr << "Error: " << err.what() << ", error code: " << err.code() << '\n'
                  << "Server diagnostics: " << err.get_diagnostics().server_message() << std::endl;
        return 1;
    }
    catch (const std::exception& err)
    {
        std::cerr << "Error: " << err.what() << std::endl;
        return 1;
    }
}

//]

#else

#include <iostream>

int main()
{
    std::cout << "Sorry, your compiler doesn't have the required capabilities to run this example"
              << std::endl;
}

#endif