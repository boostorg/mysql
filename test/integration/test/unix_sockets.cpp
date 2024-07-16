//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/connection.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/unix.hpp>
#include <boost/mysql/unix_ssl.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/test/tools/detail/per_element_manip.hpp>
#include <boost/test/unit_test.hpp>

#include "test_common/as_netres.hpp"
#include "test_common/ci_server.hpp"
#include "test_common/create_basic.hpp"
#include "test_integration/any_connection_fixture.hpp"
#include "test_integration/common.hpp"
#include "test_integration/server_features.hpp"

// Spotcheck: we can connect to the server using unix sockets,
// and use queries and prepared statements.
// old unix_connection and unix_ssl_connection works

#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS

using namespace boost::mysql;
using namespace boost::mysql::test;
namespace asio = boost::asio;
using boost::test_tools::per_element;

// Cover all possible execution requests for execute() and async_execute()

namespace {

BOOST_TEST_DECORATOR(*run_if(&server_features::unix_sockets))
BOOST_AUTO_TEST_SUITE(test_unix_sockets)

template <class Connection>
void do_test(Connection& conn)
{
    // We can prepare statements
    auto stmt = conn.async_prepare_statement("SELECT ?", as_netresult).get();
    BOOST_TEST(stmt.num_params() == 1u);

    // We can execute queries
    results r;
    conn.async_execute("SELECT 'abc'", r, as_netresult).validate_no_error();
    BOOST_TEST(r.rows() == makerows(1, "abc"), per_element());

    // We can execute statements
    conn.async_execute(stmt.bind(42), r, as_netresult).validate_no_error();
    BOOST_TEST(r.rows() == makerows(1, 42), per_element());

    // We can get errors
    conn.async_execute("SELECT * FROM bad_table", r, as_netresult)
        .validate_error(
            common_server_errc::er_no_such_table,
            "Table 'boost_mysql_integtests.bad_table' doesn't exist"
        );

    // We can terminate the connection
    conn.async_close(as_netresult).validate_no_error();
}

BOOST_FIXTURE_TEST_CASE(any_connection_, any_connection_fixture)
{
    // Connect
    conn.async_connect(connect_params_builder().set_unix().build(), as_netresult).validate_no_error();
    BOOST_TEST(!conn.uses_ssl());

    // The connection is usable
    do_test(conn);
}

BOOST_AUTO_TEST_CASE(unix_connection_)
{
    // Setup
    asio::io_context ctx;
    unix_connection conn(ctx);

    // Connect
    handshake_params params(integ_user, integ_passwd, integ_db);
    conn.async_connect(asio::local::stream_protocol::endpoint(default_unix_path), params, as_netresult)
        .validate_no_error();
    BOOST_TEST(!conn.uses_ssl());

    // The connection is usable
    do_test(conn);
}
BOOST_AUTO_TEST_CASE(unix_ssl_connection_)
{
    // Setup
    asio::io_context ctx;
    asio::ssl::context ssl_ctx(asio::ssl::context::tlsv13_client);
    unix_ssl_connection conn(ctx, ssl_ctx);

    // Connect
    handshake_params params(integ_user, integ_passwd, integ_db);
    conn.async_connect(asio::local::stream_protocol::endpoint(default_unix_path), params, as_netresult)
        .validate_no_error();
    BOOST_TEST(conn.uses_ssl());

    // The connection is usable
    do_test(conn);
}

// Handshake and quit work for UNIX connections
BOOST_AUTO_TEST_CASE(unix_connection_handshake_quit)
{
    // Setup
    asio::io_context ctx;
    unix_connection conn(ctx);

    // Connect stream
    conn.stream().connect(asio::local::stream_protocol::endpoint(default_unix_path));

    // Handshake
    handshake_params params(integ_user, integ_passwd, integ_db);
    conn.async_handshake(params, as_netresult).validate_no_error();
    BOOST_TEST(!conn.uses_ssl());

    // Quit works
    conn.async_quit(as_netresult).validate_no_error();
}

BOOST_AUTO_TEST_CASE(unix_ssl_connection_handshake_quit)
{
    // Setup
    asio::io_context ctx;
    asio::ssl::context ssl_ctx(asio::ssl::context::tls_client);
    unix_ssl_connection conn(ctx, ssl_ctx);

    // Connect the stream
    conn.stream().lowest_layer().connect(asio::local::stream_protocol::endpoint(default_unix_path));

    // Handshake
    handshake_params params(integ_user, integ_passwd, integ_db);
    conn.async_handshake(params, as_netresult).validate_no_error();
    BOOST_TEST(conn.uses_ssl());

    // Quit works
    conn.async_quit(as_netresult).validate_no_error();
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace

#endif