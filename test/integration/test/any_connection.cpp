//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/ssl_mode.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>

#include "test_common/netfun_maker.hpp"
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

connect_params create_params()
{
    connect_params res;
    res.server_address.emplace_host_and_port(get_hostname());
    res.username = default_user;
    res.password = default_passwd;
    res.database = default_db;
    return res;
}

// Passing no SSL context to the constructor and using SSL works.
// ssl_mode::require works
BOOST_AUTO_TEST_CASE(default_ssl_context)
{
    // Create the connection
    boost::asio::io_context ctx;
    any_connection conn(ctx);

    // Force using SSL
    auto params = create_params();
    params.ssl = ssl_mode::require;

    // Call the function
    connect_fn(conn, params).validate_no_error();

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

    // Force using SSL
    auto params = create_params();
    params.ssl = ssl_mode::require;

    // Certificate validation fails
    auto result = connect_fn(conn, params);
    BOOST_TEST(result.err.message().find("certificate verify failed") != std::string::npos);
}

/**
 * Passing a default SSL context works
 * Passing a custom SSL context works (certificate validation)
 * Disabling SSL works
 * ssl::enable works
 * UNIX connections don't use SSL
 * UNIX connections can use caching_sha2_password
 */

BOOST_AUTO_TEST_SUITE_END()