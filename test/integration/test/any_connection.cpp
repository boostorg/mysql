//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/common_server_errc.hpp>
#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/ssl_mode.hpp>

#include <boost/asio/deferred.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>

#include <memory>

#include "test_common/create_diagnostics.hpp"
#include "test_common/netfun_helpers.hpp"
#include "test_common/netfun_maker.hpp"
#include "test_common/network_result.hpp"
#include "test_integration/common.hpp"
#include "test_integration/get_endpoint.hpp"
#include "test_integration/server_ca.hpp"

// Additional spotchecks for any_connection

using namespace boost::mysql;
using namespace boost::mysql::test;
namespace asio = boost::asio;

BOOST_AUTO_TEST_SUITE(test_any_connection)

using netmaker_connect = netfun_maker_mem<void, any_connection, const connect_params&>;

// Don't validate executor info, since our I/O objects don't use tracker executors
const auto connect_fn = netmaker_connect::async_errinfo(&any_connection::async_connect, false);

connect_params create_params(ssl_mode mode = ssl_mode::enable)
{
    connect_params res;
    res.server_address.emplace_host_and_port(get_hostname());
    res.username = default_user;
    res.password = default_passwd;
    res.database = default_db;
    res.ssl = mode;
    return res;
}

// Passing no SSL context to the constructor and using SSL works.
// ssl_mode::require works
BOOST_AUTO_TEST_CASE(default_ssl_context)
{
    // Create the connection
    boost::asio::io_context ctx;
    any_connection conn(ctx);

    // Call the function
    connect_fn(conn, create_params(ssl_mode::require)).validate_no_error();

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
    auto result = connect_fn(conn, create_params(ssl_mode::require));
    BOOST_TEST(result.err.message().find("certificate verify failed") != std::string::npos);
}

// Disabling SSL works with TCP connections
BOOST_AUTO_TEST_CASE(tcp_ssl_mode_disable)
{
    // Create the connection
    boost::asio::io_context ctx;
    any_connection conn(ctx);

    // Call the function
    connect_fn(conn, create_params(ssl_mode::disable)).validate_no_error();

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
    connect_fn(conn, create_params(ssl_mode::enable)).validate_no_error();

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
    auto params = create_params(ssl_mode::require);
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
    auto params = create_params(ssl_mode::require);
    params.username = "csha2p_user";
    params.password = "csha2p_password";

    // Call the function
    connect_fn(conn, params).validate_no_error();
    BOOST_TEST(conn.uses_ssl());
}

// Users can log-in using the caching_sha2_password auth plugin
// even if they're using UNIX sockets
// Re-enable this test when we code https://github.com/boostorg/mysql/issues/189
// BOOST_TEST_DECORATOR(*boost::unit_test::label("unix"))
// BOOST_TEST_DECORATOR(*boost::unit_test::label("skip_mysql5"))
// BOOST_TEST_DECORATOR(*boost::unit_test::label("skip_mariadb"))
// BOOST_AUTO_TEST_CASE(unix_caching_sha2_password)
// {
//     // Create the connection
//     boost::asio::io_context ctx;
//     any_connection conn(ctx);

//     // Connect params
//     auto params = create_params();
//     params.server_address.emplace_unix_path(default_unix_path);
//     params.ssl = ssl_mode::require;
//     params.username = "csha2p_user";
//     params.password = "csha2p_password";

//     // Call the function
//     connect_fn(conn, params).validate_no_error();
//     BOOST_TEST(!conn.uses_ssl());
// }

network_result<void> create_net_result()
{
    return network_result<void>(
        common_server_errc::er_aborting_connection,
        create_server_diag("diagnostics not cleared")
    );
}

// TODO: move this to unit once we implement unit tests for handshake/connect
BOOST_AUTO_TEST_CASE(async_connect_lifetimes)
{
    // Create the connection
    boost::asio::io_context ctx;
    any_connection conn(ctx);

    // Create params with SSL disabled to save runtime
    std::unique_ptr<connect_params> params(new connect_params(create_params(ssl_mode::disable)));

    // Launch the function
    auto res = create_net_result();
    conn.async_connect(*params, *res.diag, [&](error_code ec) { res.err = ec; });

    // Make the passed-in params invalid
    params.reset();

    // Run the function until completion
    ctx.run();

    // No error
    res.validate_no_error();
}

BOOST_AUTO_TEST_CASE(async_connect_deferred_lifetimes)
{
    // Create the connection
    boost::asio::io_context ctx;
    any_connection conn(ctx);

    // Create params with SSL disabled to save runtime
    std::unique_ptr<connect_params> params(new connect_params(create_params(ssl_mode::disable)));

    // Create a deferred object
    auto res = create_net_result();
    auto op = conn.async_connect(*params, *res.diag, asio::deferred);

    // Make the params invalid
    params.reset();

    // Run the operation
    std::move(op)([&](error_code ec) { res.err = ec; });
    ctx.run();

    // No error
    res.validate_no_error();
}

BOOST_AUTO_TEST_SUITE_END()