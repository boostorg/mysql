//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "boost/mysql/connection.hpp"
#include "integration_test_common.hpp"
#include "test_common.hpp"

// Tests containing with label 'sha256' require SHA256 functionality.
// This is used by the build script to exclude these tests on databases
// that do not support this functionality

namespace net = boost::asio;
using namespace boost::mysql::test;

using boost::mysql::socket_connection;
using boost::mysql::ssl_mode;
using boost::mysql::ssl_options;
using boost::mysql::detail::make_error_code;
using boost::mysql::error_info;
using boost::mysql::errc;
using boost::mysql::error_code;
using boost::mysql::tcp_connection;
using boost::mysql::connection_params;

BOOST_AUTO_TEST_SUITE(test_handshake)

template <typename Stream>
network_result<no_result> do_handshake(
    socket_connection<Stream>& conn,
    connection_params params,
    network_functions<Stream>* net,
    ssl_mode ssl
)
{
    params.set_ssl(boost::mysql::ssl_options(ssl));
    return net->handshake(conn, params);
}

template <typename Stream>
void do_handshake_ok(
    socket_connection<Stream>& conn,
    connection_params params,
    network_functions<Stream>* net,
    ssl_mode ssl
)
{
    auto result = do_handshake(conn, params, net, ssl);
    result.validate_no_error();
    validate_ssl(conn, ssl);
}


// Handshake tests not depending on whether we use SSL or not
template <typename Stream>
struct handshake_fixture : network_fixture<Stream>
{
    handshake_fixture()
    {
        this->physical_connect();
    }
};

// mysql_native_password
BOOST_AUTO_TEST_SUITE(mysql_native_password)

BOOST_MYSQL_NETWORK_TEST(regular_user, handshake_fixture, network_ssl_gen)
{
    this->set_credentials("mysqlnp_user", "mysqlnp_password");
    do_handshake_ok(this->conn, this->params, sample.net, sample.ssl);
}

BOOST_MYSQL_NETWORK_TEST(empty_password, handshake_fixture, network_ssl_gen)
{
    this->set_credentials("mysqlnp_empty_password_user", "");
    do_handshake_ok(this->conn, this->params, sample.net, sample.ssl);
}

BOOST_MYSQL_NETWORK_TEST(bad_password, handshake_fixture, network_ssl_gen)
{
    this->set_credentials("mysqlnp_user", "bad_password");
    auto result = do_handshake(this->conn, this->params, sample.net, sample.ssl);
    result.validate_error(errc::access_denied_error, {"access denied", "mysqlnp_user"});
}

BOOST_AUTO_TEST_SUITE_END() // mysql_native_password

// caching_sha2_password
BOOST_TEST_DECORATOR(*boost::unit_test::label("sha256"))
BOOST_AUTO_TEST_SUITE(caching_sha2_password)

template <typename Stream>
struct caching_sha2_fixture : handshake_fixture<Stream>
{
    void load_sha256_cache(boost::string_view user, boost::string_view password)
    {
        tcp_connection conn (this->ctx);
        conn.connect(
            get_endpoint<boost::asio::ip::tcp::socket>(endpoint_kind::localhost),
            connection_params(user, password)
        );
        conn.close();
    }

    void clear_sha256_cache()
    {
        tcp_connection conn (this->ctx);
        conn.connect(
            get_endpoint<boost::asio::ip::tcp::socket>(endpoint_kind::localhost),
            connection_params("root", "")
        );
        conn.query("FLUSH PRIVILEGES");
        conn.close();
    }
};

BOOST_MYSQL_NETWORK_TEST(ssl_on_cache_hit, caching_sha2_fixture, network_gen)
{
    this->set_credentials("csha2p_user", "csha2p_password");
    this->load_sha256_cache("csha2p_user", "csha2p_password");
    do_handshake_ok(this->conn, this->params, sample.net, ssl_mode::require);
}

BOOST_MYSQL_NETWORK_TEST(ssl_off_cache_hit, caching_sha2_fixture, network_gen)
{
    // As we are sending password hashed, it is OK to not have SSL for this
    this->set_credentials("csha2p_user", "csha2p_password");
    this->load_sha256_cache("csha2p_user", "csha2p_password");
    do_handshake_ok(this->conn, this->params, sample.net, ssl_mode::disable);
}

BOOST_MYSQL_NETWORK_TEST(ssl_on_cache_miss, caching_sha2_fixture, network_gen)
{
    this->set_credentials("csha2p_user", "csha2p_password");
    this->clear_sha256_cache();
    do_handshake_ok(this->conn, this->params, sample.net, ssl_mode::require);
}

BOOST_MYSQL_NETWORK_TEST(ssl_off_cache_miss, caching_sha2_fixture, network_gen)
{
    // A cache miss would force us send a plaintext password over
    // a non-TLS connection, so we fail
    this->set_credentials("csha2p_user", "csha2p_password");
    this->clear_sha256_cache();
    auto result = do_handshake(this->conn, this->params, sample.net, ssl_mode::disable);
    result.validate_error(errc::auth_plugin_requires_ssl, {});
}

BOOST_MYSQL_NETWORK_TEST(empty_password_ssl_on_cache_hit, caching_sha2_fixture, network_gen)
{
    this->set_credentials("csha2p_empty_password_user", "");
    this->load_sha256_cache("csha2p_empty_password_user", "");
    do_handshake_ok(this->conn, this->params, sample.net, ssl_mode::require);
}

BOOST_MYSQL_NETWORK_TEST(empty_password_ssl_off_cache_hit, caching_sha2_fixture, network_gen)
{
    // Empty passwords are allowed over non-TLS connections
    this->set_credentials("csha2p_empty_password_user", "");
    this->load_sha256_cache("csha2p_empty_password_user", "");
    do_handshake_ok(this->conn, this->params, sample.net, ssl_mode::disable);
}

BOOST_MYSQL_NETWORK_TEST(empty_password_ssl_on_cache_miss, caching_sha2_fixture, network_gen)
{
    this->set_credentials("csha2p_empty_password_user", "");
    this->clear_sha256_cache();
    do_handshake_ok(this->conn, this->params, sample.net, ssl_mode::require);
}

BOOST_MYSQL_NETWORK_TEST(empty_password_ssl_off_cache_miss, caching_sha2_fixture, network_gen)
{
    // Empty passwords are allowed over non-TLS connections
    this->set_credentials("csha2p_empty_password_user", "");
    this->clear_sha256_cache();
    do_handshake_ok(this->conn, this->params, sample.net, ssl_mode::disable);
}

BOOST_MYSQL_NETWORK_TEST(bad_password_ssl_on_cache_hit, caching_sha2_fixture, network_gen)
{
    // Note: test over non-TLS would return "ssl required"
    this->set_credentials("csha2p_user", "bad_password");
    this->load_sha256_cache("csha2p_user", "csha2p_password");
    auto result = do_handshake(this->conn, this->params, sample.net, ssl_mode::require);
    result.validate_error(errc::access_denied_error, {"access denied", "csha2p_user"});
}

BOOST_MYSQL_NETWORK_TEST(bad_password_ssl_on_cache_miss, caching_sha2_fixture, network_gen)
{
    // Note: test over non-TLS would return "ssl required"
    this->set_credentials("csha2p_user", "bad_password");
    this->clear_sha256_cache();
    auto result = do_handshake(this->conn, this->params, sample.net, ssl_mode::require);
    result.validate_error(errc::access_denied_error, {"access denied", "csha2p_user"});
}

BOOST_AUTO_TEST_SUITE_END() // caching_sha2_password

// Other handshake tests
BOOST_MYSQL_NETWORK_TEST(no_database, handshake_fixture, network_ssl_gen)
{
    this->params.set_database("");
    do_handshake_ok(this->conn, this->params, sample.net, sample.ssl);
}

BOOST_MYSQL_NETWORK_TEST(bad_database, handshake_fixture, network_ssl_gen)
{
    this->params.set_database("bad_database");
    auto result = do_handshake(this->conn, this->params, sample.net, sample.ssl);
    result.validate_error(errc::dbaccess_denied_error, {"database", "bad_database"});
}

BOOST_TEST_DECORATOR(*boost::unit_test::label("sha256"))
BOOST_MYSQL_NETWORK_TEST(unknown_auth_plugin, handshake_fixture, network_ssl_gen)
{
    // Note: sha256_password is not supported, so it's an unknown plugin to us
    this->set_credentials("sha2p_user", "sha2p_password");
    auto result = do_handshake(this->conn, this->params, sample.net, sample.ssl);
    result.validate_error(errc::unknown_auth_plugin, {});
}

BOOST_MYSQL_NETWORK_TEST(bad_user, handshake_fixture, network_gen)
{
    // unreliable without SSL. If the default plugin requires SSL
    // (like SHA256), this would fail with 'ssl required'
    this->set_credentials("non_existing_user", "bad_password");
    auto result = do_handshake(this->conn, this->params, sample.net, ssl_mode::require);
    result.validate_any_error(); // may be access denied or unknown auth plugin
}

BOOST_MYSQL_NETWORK_TEST(ssl_enable, handshake_fixture, network_gen)
{
    // In all our CI systems, our servers support SSL, so
    // ssl_mode::enable will do the same as ssl_mode::require.
    // We test for this fact.
    do_handshake_ok(this->conn, this->params, sample.net, ssl_mode::enable);
}

BOOST_AUTO_TEST_SUITE_END() // test_handshake
