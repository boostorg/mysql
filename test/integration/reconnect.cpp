//
// Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/ssl/verify_mode.hpp>
#include <boost/mysql/socket_connection.hpp>
#include <boost/test/tools/interface.hpp>
#include <boost/test/unit_test_suite.hpp>
#include "boost/mysql/connection_params.hpp"
#include "boost/mysql/errc.hpp"
#include "get_endpoint.hpp"
#include "integration_test_common.hpp"
#include "network_functions.hpp"

using namespace boost::mysql::test;
using boost::mysql::ssl_mode;

BOOST_AUTO_TEST_SUITE(test_reconnect)

template <class Stream>
struct reconnect_fixture : network_fixture<Stream>
{
    using network_fixture<Stream>::network_fixture;

    void do_connect_ok(network_functions<Stream>* net, ssl_mode m)
    {
        this->params.set_ssl(m);
        auto result = net->connect(this->conn, get_endpoint<Stream>(endpoint_kind::localhost), this->params);
        result.validate_no_error();
        validate_ssl(this->conn, m);
    }

    void do_query_ok(network_functions<Stream>* net)
    {
        auto result = net->query(this->conn, "SELECT * FROM empty_table");
        result.validate_no_error();
        auto read_result = net->read_all(result.value);
        read_result.validate_no_error();
        BOOST_TEST(read_result.value.empty());
    }
};

template <class Stream>
struct reconnect_fixture_external_ctx : reconnect_fixture<Stream>
{
    reconnect_fixture_external_ctx(): reconnect_fixture<Stream>(use_external_ctx) {}
};

// Using default SSL context
BOOST_AUTO_TEST_SUITE(default_ssl_ctx)

BOOST_MYSQL_NETWORK_TEST(reconnect_after_close, reconnect_fixture, network_ssl_gen)
{
    // Connect and use the connection
    this->do_connect_ok(sample.net, sample.ssl);
    this->do_query_ok(sample.net);

    // Close
    auto result = sample.net->close(this->conn);
    result.validate_no_error();

    // Reopen and use the connection normally
    this->do_connect_ok(sample.net, sample.ssl);
    this->do_query_ok(sample.net);
}

BOOST_MYSQL_NETWORK_TEST(reconnect_after_handshake_error, reconnect_fixture, network_ssl_gen)
{
    // Error during server handshake
    this->params.set_ssl(sample.ssl);
    this->params.set_database("bad_database");
    auto result = sample.net->connect(this->conn, get_endpoint<Stream>(endpoint_kind::localhost), this->params);
    result.validate_error(boost::mysql::errc::dbaccess_denied_error, {"database", "bad_database"});

    // Reopen with correct parameters and use the connection normally
    this->params.set_database("boost_mysql_integtests");
    this->do_connect_ok(sample.net, sample.ssl);
    this->do_query_ok(sample.net);
}

BOOST_MYSQL_NETWORK_TEST(reconnect_after_physical_connect_error, reconnect_fixture, network_ssl_gen)
{
    // Error during connection
    this->params.set_ssl(sample.ssl);
    auto result = sample.net->connect(this->conn, get_endpoint<Stream>(endpoint_kind::inexistent), this->params);
    result.validate_any_error({"physical connect failed"});

    // Reopen with and use the connection normally
    this->do_connect_ok(sample.net, sample.ssl);
    this->do_query_ok(sample.net);
}

BOOST_AUTO_TEST_SUITE_END()

// Using custom SSL context
BOOST_AUTO_TEST_SUITE(external_ssl_ctx)

BOOST_MYSQL_NETWORK_TEST(reconnect_after_close, reconnect_fixture_external_ctx, network_ssl_gen)
{
    // Connect and use the connection
    this->do_connect_ok(sample.net, sample.ssl);
    this->do_query_ok(sample.net);

    // Close
    auto result = sample.net->close(this->conn);
    result.validate_no_error();

    // Reopen and use the connection normally
    this->do_connect_ok(sample.net, sample.ssl);
    this->do_query_ok(sample.net);
}

BOOST_MYSQL_NETWORK_TEST(reconnect_after_handshake_error, reconnect_fixture_external_ctx, network_ssl_gen)
{
    // Error during server handshake
    this->params.set_ssl(sample.ssl);
    this->params.set_database("bad_database");
    auto result = sample.net->connect(this->conn, get_endpoint<Stream>(endpoint_kind::localhost), this->params);
    result.validate_error(boost::mysql::errc::dbaccess_denied_error, {"database", "bad_database"});

    // Reopen with correct parameters and use the connection normally
    this->params.set_database("boost_mysql_integtests");
    this->do_connect_ok(sample.net, sample.ssl);
    this->do_query_ok(sample.net);
}

BOOST_MYSQL_NETWORK_TEST(reconnect_after_physical_connect_error, reconnect_fixture_external_ctx, network_ssl_gen)
{
    // Error during connection
    this->params.set_ssl(sample.ssl);
    auto result = sample.net->connect(this->conn, get_endpoint<Stream>(endpoint_kind::inexistent), this->params);
    result.validate_any_error({"physical connect failed"});

    // Reopen with and use the connection normally
    this->do_connect_ok(sample.net, sample.ssl);
    this->do_query_ok(sample.net);
}

BOOST_MYSQL_NETWORK_TEST(reconnect_after_ssl_handshake_error, reconnect_fixture_external_ctx, network_gen)
{
    // Error during SSL certificate validation
    this->external_ctx.set_verify_mode(boost::asio::ssl::verify_peer);
    this->params.set_ssl(boost::mysql::ssl_mode::require);
    auto result = sample.net->connect(this->conn, get_endpoint<Stream>(endpoint_kind::localhost), this->params);
    BOOST_TEST(result.err.message() == "certificate verify failed");

    // Disable certificate validation and use the connection normally
    this->external_ctx.set_verify_mode(boost::asio::ssl::verify_none);
    this->do_connect_ok(sample.net, boost::mysql::ssl_mode::require);
    this->do_query_ok(sample.net);
}

BOOST_AUTO_TEST_SUITE_END() // external ssl ctx

BOOST_AUTO_TEST_SUITE_END() // test_reconnect

