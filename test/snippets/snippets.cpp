//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// This file contains all the snippets that are used in the docs.
// They're here so they are built and run, to ensure correctness

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/character_set.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/connection.hpp>
#include <boost/mysql/connection_pool.hpp>
#include <boost/mysql/date.hpp>
#include <boost/mysql/datetime.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/field.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/format_sql.hpp>
#include <boost/mysql/metadata_mode.hpp>
#include <boost/mysql/pool_params.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/resultset.hpp>
#include <boost/mysql/resultset_view.hpp>
#include <boost/mysql/row.hpp>
#include <boost/mysql/row_view.hpp>
#include <boost/mysql/rows.hpp>
#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/ssl_mode.hpp>
#include <boost/mysql/statement.hpp>
#include <boost/mysql/static_execution_state.hpp>
#include <boost/mysql/static_results.hpp>
#include <boost/mysql/string_view.hpp>
#include <boost/mysql/tcp_ssl.hpp>
#include <boost/mysql/throw_on_error.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/asio/ssl/verify_mode.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/config.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/core/span.hpp>
#include <boost/describe/class.hpp>
#include <boost/optional/optional.hpp>
#include <boost/system/system_error.hpp>

#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <tuple>

#include "test_common/ci_server.hpp"

#ifndef BOOST_NO_CXX17_HDR_OPTIONAL
#include <optional>
#endif
#ifdef BOOST_ASIO_HAS_CO_AWAIT
#include <boost/asio/experimental/awaitable_operators.hpp>
#endif
#ifdef __cpp_lib_polymorphic_allocator
#include <memory_resource>
#endif

using boost::mysql::date;
using boost::mysql::datetime;
using boost::mysql::diagnostics;
using boost::mysql::error_code;
using boost::mysql::error_with_diagnostics;
using boost::mysql::execution_state;
using boost::mysql::field;
using boost::mysql::field_view;
using boost::mysql::metadata_mode;
using boost::mysql::results;
using boost::mysql::row;
using boost::mysql::row_view;
using boost::mysql::rows;
using boost::mysql::rows_view;
using boost::mysql::statement;
using boost::mysql::string_view;
using boost::mysql::tcp_ssl_connection;
#ifdef BOOST_MYSQL_CXX14
using boost::mysql::static_execution_state;
using boost::mysql::static_results;
#endif

#define ASSERT(expr)                                          \
    if (!(expr))                                              \
    {                                                         \
        std::cerr << "Assertion failed: " #expr << std::endl; \
        exit(1);                                              \
    }

// Connection params
std::string server_hostname = boost::mysql::test::get_hostname();
static constexpr const char* mysql_username = "example_user";
static constexpr const char* mysql_password = "example_password";

#ifdef BOOST_ASIO_HAS_CO_AWAIT
void run_coro(boost::asio::any_io_executor ex, std::function<boost::asio::awaitable<void>(void)> fn)
{
    boost::asio::co_spawn(ex, fn, [](std::exception_ptr ptr) {
        if (ptr)
        {
            std::rethrow_exception(ptr);
        }
    });
    static_cast<boost::asio::io_context&>(ex.context()).run();
}
#endif

void main_impl()
{
    //
    // setup and connect - this is included in overview, too
    //

    //[overview_connection
    // The execution context, required to run I/O operations.
    boost::asio::io_context ctx;

    // The SSL context, required to establish TLS connections.
    // The default SSL options are good enough for us at this point.
    boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::tls_client);

    // Represents a connection to the MySQL server.
    boost::mysql::tcp_ssl_connection conn(ctx.get_executor(), ssl_ctx);
    //]

    //[overview_connect
    // Resolve the hostname to get a collection of endpoints
    boost::asio::ip::tcp::resolver resolver(ctx.get_executor());
    auto endpoints = resolver.resolve(server_hostname, boost::mysql::default_port_string);

    // The username and password to use
    boost::mysql::handshake_params params(
        mysql_username,         // username
        mysql_password,         // password
        "boost_mysql_examples"  // database
    );

    // Connect to the server using the first endpoint returned by the resolver
    conn.connect(*endpoints.begin(), params);
    //]

    section_multi_resultset(conn);
    section_multi_resultset_multi_queries();
    section_multi_function(conn);
    section_metadata(conn);
    section_charsets(conn);
    section_time_types(conn);
    section_any_connection();
    section_connection_pool();
    section_sql_formatting();

    conn.close();
}

int main()
{
    try
    {
        main_impl();
    }
    catch (const boost::mysql::error_with_diagnostics& err)
    {
        std::cerr << "Error: " << err.what() << '\n'
                  << "Server diagnostics: " << err.get_diagnostics().server_message() << std::endl;
        return 1;
    }
    catch (const std::exception& err)
    {
        std::cerr << "Error: " << err.what() << std::endl;
        return 1;
    }
}
