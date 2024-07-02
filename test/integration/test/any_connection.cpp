//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/common_server_errc.hpp>
#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/format_sql.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/ssl_mode.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/engine_impl.hpp>

#include <boost/mysql/impl/internal/variant_stream.hpp>

#include <boost/asio/deferred.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>

#include <string>

#include "test_common/create_basic.hpp"
#include "test_common/create_diagnostics.hpp"
#include "test_common/netfun_maker.hpp"
#include "test_common/network_result.hpp"
#include "test_common/printing.hpp"
#include "test_integration/common.hpp"
#include "test_integration/server_ca.hpp"

// Additional spotchecks for any_connection

using namespace boost::mysql;
using namespace boost::mysql::test;
namespace asio = boost::asio;
using boost::test_tools::per_element;

BOOST_AUTO_TEST_SUITE(test_any_connection)

using netmaker_connect = netfun_maker_mem<void, any_connection, const connect_params&>;
using netmaker_execute = netfun_maker_mem<void, any_connection, const string_view&, results&>;

// Don't validate executor info, since our I/O objects don't use tracker executors
const auto connect_fn = netmaker_connect::async_errinfo(&any_connection::async_connect, false);
const auto execute_fn = netmaker_execute::async_errinfo(&any_connection::async_execute, false);

// Passing no SSL context to the constructor and using SSL works.
// ssl_mode::require works
BOOST_AUTO_TEST_CASE(default_ssl_context)
{
    // Create the connection
    boost::asio::io_context ctx;
    any_connection conn(ctx);

    // Call the function
    connect_fn(conn, default_connect_params(ssl_mode::require)).validate_no_error();

    // uses_ssl reports the right value
    BOOST_TEST(conn.uses_ssl());
}

// Passing a custom SSL context works
BOOST_AUTO_TEST_CASE(custom_ssl_context)
{
    // Set up a SSL context
    asio::ssl::context ssl_ctx(asio::ssl::context::tlsv12_client);
    ssl_ctx.set_verify_mode(boost::asio::ssl::verify_peer);
    ssl_ctx.add_certificate_authority(boost::asio::buffer(CA_PEM));
    ssl_ctx.set_verify_callback(boost::asio::ssl::host_name_verification("bad.host.name"));

    // Create the connection
    asio::io_context ctx;
    any_connection_params ctor_params;
    ctor_params.ssl_context = &ssl_ctx;
    any_connection conn(ctx, ctor_params);

    // Certificate validation fails
    auto result = connect_fn(conn, default_connect_params(ssl_mode::require));
    BOOST_TEST(result.err.message().find("certificate verify failed") != std::string::npos);
}

// Disabling SSL works with TCP connections
BOOST_AUTO_TEST_CASE(tcp_ssl_mode_disable)
{
    // Create the connection
    boost::asio::io_context ctx;
    any_connection conn(ctx);

    // Call the function
    connect_fn(conn, default_connect_params(ssl_mode::disable)).validate_no_error();

    // uses_ssl reports the right value
    BOOST_TEST(!conn.uses_ssl());
}

// SSL mode enable woks with TCP connections
BOOST_AUTO_TEST_CASE(tcp_ssl_mode_enable)
{
    // Create the connection
    boost::asio::io_context ctx;
    any_connection conn(ctx);

    // Call the function
    connect_fn(conn, default_connect_params(ssl_mode::enable)).validate_no_error();

    // All our CIs support SSL
    BOOST_TEST(conn.uses_ssl());
}

// UNIX connections never use SSL
BOOST_TEST_DECORATOR(*boost::unit_test::label("unix"))
BOOST_AUTO_TEST_CASE(unix_ssl)
{
    // Create the connection
    boost::asio::io_context ctx;
    any_connection conn(ctx);

    // Connect params
    auto params = default_connect_params(ssl_mode::require);
    params.server_address.emplace_unix_path(default_unix_path);

    // Call the function
    connect_fn(conn, params).validate_no_error();

    // SSL is not enabled even if we specified require, since there's
    // no point in using SSL with UNIX sockets
    BOOST_TEST(!conn.uses_ssl());
}

// Spotcheck: users can log-in using the caching_sha2_password auth plugin
BOOST_TEST_DECORATOR(*boost::unit_test::label("skip_mysql5"))
BOOST_TEST_DECORATOR(*boost::unit_test::label("skip_mariadb"))
BOOST_AUTO_TEST_CASE(tcp_caching_sha2_password)
{
    // Create the connection
    boost::asio::io_context ctx;
    any_connection conn(ctx);

    // Connect params
    auto params = default_connect_params(ssl_mode::require);
    params.username = "csha2p_user";
    params.password = "csha2p_password";

    // Call the function
    connect_fn(conn, params).validate_no_error();
    BOOST_TEST(conn.uses_ssl());
}

// Users can log-in using the caching_sha2_password auth plugin
// even if they're using UNIX sockets.
BOOST_TEST_DECORATOR(*boost::unit_test::label("unix"))
BOOST_TEST_DECORATOR(*boost::unit_test::label("skip_mysql5"))
BOOST_TEST_DECORATOR(*boost::unit_test::label("skip_mariadb"))
BOOST_AUTO_TEST_CASE(unix_caching_sha2_password)
{
    // Setup
    boost::asio::io_context ctx;
    any_connection conn(ctx);
    any_connection root_conn(ctx);

    // Clear the sha256 cache. This forces us send the password using plain text.
    // TODO: we could create a dedicated user - this can make the test more reliable
    auto root_params = default_connect_params();
    results r;
    root_params.username = "root";
    root_params.password = "";
    root_conn.connect(root_params);
    root_conn.execute("FLUSH PRIVILEGES", r);

    // Connect params
    auto params = default_connect_params(ssl_mode::require);
    params.server_address.emplace_unix_path(default_unix_path);
    params.username = "csha2p_user";
    params.password = "csha2p_password";

    // Call the function
    connect_fn(conn, params).validate_no_error();
    BOOST_TEST(!conn.uses_ssl());
}

network_result<void> create_net_result()
{
    return network_result<void>(
        common_server_errc::er_aborting_connection,
        create_server_diag("diagnostics not cleared")
    );
}

// Backslash escapes
BOOST_AUTO_TEST_CASE(backslash_escapes)
{
    // Create the connection
    boost::asio::io_context ctx;
    any_connection conn(ctx);

    // Backslash escapes enabled by default
    BOOST_TEST(conn.backslash_escapes());

    // Connect doesn't change the value
    connect_fn(conn, default_connect_params(ssl_mode::disable)).validate_no_error();
    BOOST_TEST(conn.backslash_escapes());
    BOOST_TEST(conn.format_opts()->backslash_escapes);

    // Setting the SQL mode to NO_BACKSLASH_ESCAPES updates the value
    results r;
    execute_fn(conn, "SET sql_mode = 'NO_BACKSLASH_ESCAPES'", r).validate_no_error();
    BOOST_TEST(!conn.backslash_escapes());
    BOOST_TEST(!conn.format_opts()->backslash_escapes);

    // Executing a different statement doesn't change the value
    execute_fn(conn, "SELECT 1", r).validate_no_error();
    BOOST_TEST(!conn.backslash_escapes());
    BOOST_TEST(!conn.format_opts()->backslash_escapes);

    // Clearing the SQL mode updates the value
    execute_fn(conn, "SET sql_mode = ''", r).validate_no_error();
    BOOST_TEST(conn.backslash_escapes());
    BOOST_TEST(conn.format_opts()->backslash_escapes);

    // Reconnecting clears the value
    execute_fn(conn, "SET sql_mode = 'NO_BACKSLASH_ESCAPES'", r).validate_no_error();
    BOOST_TEST(!conn.backslash_escapes());
    BOOST_TEST(!conn.format_opts()->backslash_escapes);
    connect_fn(conn, default_connect_params(ssl_mode::disable)).validate_no_error();
    BOOST_TEST(conn.backslash_escapes());
    BOOST_TEST(conn.format_opts()->backslash_escapes);
}

// Max buffer sizes
BOOST_AUTO_TEST_CASE(max_buffer_size)
{
    // Create the connection
    boost::asio::io_context ctx;
    any_connection_params params;
    params.initial_buffer_size = 512u;
    params.max_buffer_size = 512u;
    any_connection conn(ctx, params);

    // Connect
    connect_fn(conn, default_connect_params(ssl_mode::disable)).validate_no_error();

    // Reading and writing almost 512 bytes works
    results r;
    auto q = format_sql(conn.format_opts().value(), "SELECT {}", std::string(450, 'a'));
    execute_fn(conn, q, r).validate_no_error();
    BOOST_TEST(r.rows() == makerows(1, std::string(450, 'a')), per_element());

    // Trying to write more than 512 bytes fails
    q = format_sql(conn.format_opts().value(), "SELECT LENGTH({})", std::string(512, 'a'));
    execute_fn(conn, q, r).validate_error_exact(client_errc::max_buffer_size_exceeded);

    // Trying to read more than 512 bytes fails
    execute_fn(conn, "SELECT REPEAT('a', 512)", r)
        .validate_error_exact(client_errc::max_buffer_size_exceeded);
}

BOOST_AUTO_TEST_CASE(default_max_buffer_size_success)
{
    // Create the connection
    boost::asio::io_context ctx;
    any_connection conn(ctx);

    // Connect
    connect_fn(conn, default_connect_params(ssl_mode::disable)).validate_no_error();

    // Reading almost max_buffer_size works
    execution_state st;
    conn.start_execution("SELECT 1, REPEAT('a', 0x3f00000)", st);
    auto rws = conn.read_some_rows(st);
    BOOST_TEST(rws.at(0).at(1).as_string().size() == 0x3f00000u);
}

BOOST_AUTO_TEST_CASE(default_max_buffer_size_error)
{
    // Create the connection
    boost::asio::io_context ctx;
    any_connection conn(ctx);

    // Connect
    connect_fn(conn, default_connect_params(ssl_mode::disable)).validate_no_error();

    // Trying to read more than max_buffer_size bytes fails
    results r;
    execute_fn(conn, "SELECT 1, REPEAT('a', 0x4000000)", r)
        .validate_error_exact(client_errc::max_buffer_size_exceeded);
}

BOOST_AUTO_TEST_CASE(naggle_disabled)
{
    struct
    {
        string_view name;
        netmaker_connect::signature fn;
    } test_cases[] = {
        {"sync",  netmaker_connect::sync_errc(&any_connection::connect)},
        {"async", connect_fn                                           },
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            // Create the connection
            boost::asio::io_context ctx;
            any_connection conn(ctx);

            // Connect
            tc.fn(conn, default_connect_params(ssl_mode::disable)).validate_no_error();

            // Naggle's algorithm was disabled
            asio::ip::tcp::no_delay opt;
            static_cast<detail::engine_impl<detail::variant_stream>&>(
                detail::access::get_impl(conn).get_engine()
            )
                .stream()
                .tcp_socket()
                .get_option(opt);
            BOOST_TEST(opt.value() == true);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()