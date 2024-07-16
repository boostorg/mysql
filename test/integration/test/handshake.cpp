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
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/ssl_mode.hpp>
#include <boost/mysql/string_view.hpp>
#include <boost/mysql/tcp.hpp>
#include <boost/mysql/tcp_ssl.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/asio/ssl/verify_mode.hpp>
#include <boost/test/data/monomorphic/collection.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/tools/context.hpp>
#include <boost/test/unit_test.hpp>

#include <utility>
#include <vector>

#include "test_common/as_netres.hpp"
#include "test_common/ci_server.hpp"
#include "test_common/create_basic.hpp"
#include "test_integration/any_connection_fixture.hpp"
#include "test_integration/common.hpp"
#include "test_integration/get_endpoint.hpp"
#include "test_integration/server_ca.hpp"
#include "test_integration/server_features.hpp"
#include "test_integration/snippets/credentials.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;
using boost::test_tools::per_element;
namespace asio = boost::asio;
namespace data = boost::unit_test::data;

namespace {

BOOST_AUTO_TEST_SUITE(test_handshake)

// TODO: we can double-check SSL using 'SHOW STATUS LIKE 'ssl_version''
// TODO: test that old tcp_ssl_connection can be passed a custom collation ID
// TODO: update multi queries to test old/new connections
// TODO: review connection termination tests

// Handshake is the most convoluted part of MySQL protocol,
// and is in active development in current MySQL versions.
// We try to test all combinations of auth methods/transports.
struct transport_test_case
{
    string_view name;
    connect_params params;
    bool expect_ssl;
};

static std::vector<transport_test_case> secure_transports()
{
    std::vector<transport_test_case> res{
        {"tcp_ssl", default_connect_params(ssl_mode::require), true},
    };

#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
    if (get_server_features().unix_sockets)
    {
        auto unix_params = default_connect_params();
        unix_params.server_address.emplace_unix_path(default_unix_path);
        res.push_back({"unix", std::move(unix_params), false});
    }
#endif

    return res;
}

static std::vector<transport_test_case> all_transports()
{
    auto res = secure_transports();
    res.push_back({"tcp", default_connect_params(ssl_mode::disable), false});
    return res;
}

// mysql_native_password
BOOST_AUTO_TEST_SUITE(mysql_native_password)

constexpr const char* regular_user = "mysqlnp_user";
constexpr const char* regular_passwd = "mysqlnp_password";
constexpr const char* empty_user = "mysqlnp_empty_password_user";

BOOST_AUTO_TEST_CASE(regular_password)
{
    for (const auto& tc : all_transports())
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            // Setup
            any_connection_fixture fix;
            auto params = tc.params;
            params.username = regular_user;
            params.password = regular_passwd;

            // Handshake succeeds
            fix.conn.async_connect(params, as_netresult).validate_no_error();
            BOOST_TEST(fix.conn.uses_ssl() == tc.expect_ssl);
        }
    }
}

BOOST_AUTO_TEST_CASE(empty_password)
{
    for (const auto& tc : all_transports())
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            // Setup
            any_connection_fixture fix;
            auto params = tc.params;
            params.username = empty_user;
            params.password = "";

            // Handshake succeeds
            fix.conn.async_connect(params, as_netresult).validate_no_error();
            BOOST_TEST(fix.conn.uses_ssl() == tc.expect_ssl);
        }
    }
}

BOOST_AUTO_TEST_CASE(bad_password)
{
    for (const auto& tc : all_transports())
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            // Setup
            any_connection_fixture fix;
            auto params = tc.params;
            params.username = regular_user;
            params.password = "bad_password";

            // Handshake fails with the expected error code
            fix.conn.async_connect(params, as_netresult)
                .validate_error_contains(
                    common_server_errc::er_access_denied_error,
                    {"access denied", regular_user}
                );
        }
    }
}

// Spotcheck: mysql_native_password works with old connection
BOOST_AUTO_TEST_CASE(tcp_connection_)
{
    // Setup
    asio::io_context ctx;
    tcp_connection conn(ctx);
    handshake_params params(regular_user, regular_passwd, integ_db);

    // Connect succeeds
    conn.async_connect(get_tcp_endpoint(), params, as_netresult).validate_no_error();
}

BOOST_AUTO_TEST_SUITE_END()  // mysql_native_password

// caching_sha2_password. We acquire a lock on the sha256_mutex
// (dummy table, used as a mutex) to avoid race conditions with other test runs
// (which happens in b2 builds).
// The sha256 cache is shared between all clients.
struct caching_sha2_lock : any_connection_fixture
{
    caching_sha2_lock()
    {
        // Connect
        auto params = default_connect_params();
        params.username = "root";
        params.password = "";
        conn.async_connect(params, as_netresult).validate_no_error();

        // Acquire the lock
        results r;
        conn.async_execute("LOCK TABLE sha256_mutex WRITE", r, as_netresult).validate_no_error();
    }

    ~caching_sha2_lock()
    {
        // Close the connection, releasing the lock.
        // TODO: the base fixture should close the connection already
        conn.async_close(as_netresult).run();
    }
};

constexpr const char* regular_user = "csha2p_user";
constexpr const char* regular_passwd = "csha2p_password";
constexpr const char* empty_user = "csha2p_empty_password_user";

BOOST_TEST_DECORATOR(*run_if(&server_features::sha256))
BOOST_AUTO_TEST_SUITE(caching_sha2_password, *boost::unit_test::fixture<caching_sha2_lock>())

static void load_sha256_cache(string_view user, string_view password)
{
    // Connecting as the given user loads the cache
    any_connection_fixture fix;
    auto params = default_connect_params();
    params.username = user;
    params.password = password;
    fix.conn.async_connect(params, as_netresult).validate_no_error();
    fix.conn.async_close(as_netresult).validate_no_error();
}

static void clear_sha256_cache()
{
    // Issuing a FLUSH PRIVILEGES clears the cache
    any_connection_fixture fix;
    connect_params params;
    params.username = "root";
    params.password = "";
    fix.conn.async_connect(params, as_netresult).validate_no_error();

    results result;
    fix.conn.async_execute("FLUSH PRIVILEGES", result, as_netresult).validate_no_error();
    fix.conn.async_close(as_netresult).validate_no_error();
};

// Cache hit means that we are sending the password hashed, so it is OK to not have SSL for this
BOOST_AUTO_TEST_CASE(cache_hit)
{
    // One-time setup
    load_sha256_cache(regular_user, regular_passwd);

    for (const auto& tc : all_transports())
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            // Setup
            any_connection_fixture fix;
            auto params = tc.params;
            params.username = regular_user;
            params.password = regular_passwd;

            // Handshake succeeds
            fix.conn.async_connect(params, as_netresult).validate_no_error();
            BOOST_TEST(fix.conn.uses_ssl() == tc.expect_ssl);
        }
    }
}

// Cache miss succeeds only if the underlying transport is secure
BOOST_AUTO_TEST_CASE(cache_miss_success)
{
    for (const auto& tc : secure_transports())
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            // Setup
            any_connection_fixture fix;
            auto params = tc.params;
            params.username = regular_user;
            params.password = regular_passwd;
            clear_sha256_cache();

            // Handshake succeeds
            fix.conn.async_connect(params, as_netresult).validate_no_error();
            BOOST_TEST(fix.conn.uses_ssl() == tc.expect_ssl);
        }
    }
}

// A cache miss would force us send a plaintext password over a non-TLS connection, so we fail
BOOST_FIXTURE_TEST_CASE(cache_miss_error, any_connection_fixture)
{
    // Setup
    auto params = default_connect_params(ssl_mode::disable);
    params.username = regular_user;
    params.password = regular_passwd;
    clear_sha256_cache();

    // Handshake fails
    conn.async_connect(params, as_netresult).validate_error(client_errc::auth_plugin_requires_ssl);
}

// Empty password users can log in regardless of the SSL usage or cache state
BOOST_AUTO_TEST_CASE(empty_password_cache_hit)
{
    // One-time setup
    load_sha256_cache(empty_user, "");

    for (const auto& tc : all_transports())
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            // Setup
            any_connection_fixture fix;
            auto params = tc.params;
            params.username = empty_user;
            params.password = "";

            // Handshake succeeds
            fix.conn.async_connect(params, as_netresult).validate_no_error();
            BOOST_TEST(fix.conn.uses_ssl() == tc.expect_ssl);
        }
    }
}

BOOST_AUTO_TEST_CASE(empty_password_cache_miss)
{
    for (const auto& tc : all_transports())
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            // Setup
            any_connection_fixture fix;
            auto params = tc.params;
            params.username = empty_user;
            params.password = "";
            clear_sha256_cache();

            // Handshake succeeds
            fix.conn.async_connect(params, as_netresult).validate_no_error();
            BOOST_TEST(fix.conn.uses_ssl() == tc.expect_ssl);
        }
    }
}

BOOST_FIXTURE_TEST_CASE(bad_password_cache_hit, any_connection_fixture)
{
    // Note: test over non-TLS would return "ssl required"
    auto params = default_connect_params(ssl_mode::require);
    params.username = regular_user;
    params.password = "bad_password";
    load_sha256_cache(regular_user, regular_passwd);
    conn.async_connect(params, as_netresult)
        .validate_error_contains(common_server_errc::er_access_denied_error, {"access denied", regular_user});
}

BOOST_FIXTURE_TEST_CASE(bad_password_cache_miss, any_connection_fixture)
{
    // Note: test over non-TLS would return "ssl required"
    auto params = default_connect_params(ssl_mode::require);
    params.username = regular_user;
    params.password = "bad_password";
    clear_sha256_cache();
    conn.async_connect(params, as_netresult)
        .validate_error_contains(common_server_errc::er_access_denied_error, {"access denied", regular_user});
}

// Spotcheck: an invalid DB error after cache miss works
BOOST_FIXTURE_TEST_CASE(bad_db_cache_miss, any_connection_fixture)
{
    // Setup
    auto params = default_connect_params(ssl_mode::require);
    params.database = "bad_db";
    clear_sha256_cache();

    // Connect fails
    conn.async_connect(params, as_netresult)
        .validate_error(
            common_server_errc::er_dbaccess_denied_error,
            "Access denied for user 'integ_user'@'%' to database 'bad_db'"
        );
}

// Spotcheck: caching_sha2_password works with old connection
BOOST_AUTO_TEST_CASE(tcp_ssl_connection_)
{
    // Setup
    asio::io_context ctx;
    asio::ssl::context ssl_ctx(asio::ssl::context::tlsv13_client);
    tcp_ssl_connection conn(ctx, ssl_ctx);
    handshake_params params(regular_user, regular_passwd, integ_db);

    // Connect succeeds
    conn.async_connect(get_tcp_endpoint(), params, as_netresult).validate_no_error();
}

BOOST_AUTO_TEST_SUITE_END()  // caching_sha2_password

// SSL certificate validation
// This also tests that we can pass a custom ssl::context to connections
BOOST_AUTO_TEST_SUITE(ssl_certificate_validation)

BOOST_AUTO_TEST_CASE(certificate_valid)
{
    // Setup
    asio::ssl::context ssl_ctx(asio::ssl::context::tlsv13_client);
    ssl_ctx.set_verify_mode(boost::asio::ssl::verify_peer);
    ssl_ctx.add_certificate_authority(boost::asio::buffer(CA_PEM));
    any_connection_fixture fix(ssl_ctx);

    // Connect works
    fix.conn.async_connect(default_connect_params(ssl_mode::require), as_netresult).validate_no_error();
    BOOST_TEST(fix.conn.uses_ssl());
}

BOOST_AUTO_TEST_CASE(certificate_invalid)
{
    // Setup
    asio::ssl::context ssl_ctx(asio::ssl::context::tlsv13_client);
    ssl_ctx.set_verify_mode(boost::asio::ssl::verify_peer);
    any_connection_fixture fix(ssl_ctx);

    // Connect fails
    auto netres = fix.conn.async_connect(default_connect_params(ssl_mode::require), as_netresult);
    netres.run();
    BOOST_TEST(netres.error().message().find("certificate verify failed") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(custom_certificate_verification_success)
{
    // Setup
    asio::ssl::context ssl_ctx(asio::ssl::context::tlsv13_client);
    ssl_ctx.set_verify_mode(boost::asio::ssl::verify_peer);
    ssl_ctx.add_certificate_authority(boost::asio::buffer(CA_PEM));
    ssl_ctx.set_verify_callback(boost::asio::ssl::host_name_verification("mysql"));
    any_connection_fixture fix(ssl_ctx);

    // Connect succeeds
    fix.conn.async_connect(default_connect_params(ssl_mode::require), as_netresult).validate_no_error();
    BOOST_TEST(fix.conn.uses_ssl());
}

BOOST_AUTO_TEST_CASE(custom_certificate_verification_error)
{
    // Setup
    asio::ssl::context ssl_ctx(asio::ssl::context::tlsv13_client);
    ssl_ctx.set_verify_mode(boost::asio::ssl::verify_peer);
    ssl_ctx.add_certificate_authority(boost::asio::buffer(CA_PEM));
    ssl_ctx.set_verify_callback(boost::asio::ssl::host_name_verification("host.name"));
    any_connection_fixture fix(ssl_ctx);

    // Connect fails
    auto netres = fix.conn.async_connect(default_connect_params(ssl_mode::require), as_netresult);
    netres.run();
    BOOST_TEST(netres.error().message().find("certificate verify failed") != std::string::npos);
}

// Spotcheck: a custom SSL context can be used with old connections
BOOST_AUTO_TEST_CASE(tcp_ssl_connection_)
{
    // Setup
    asio::ssl::context ssl_ctx(asio::ssl::context::tlsv13_client);
    ssl_ctx.set_verify_mode(boost::asio::ssl::verify_peer);
    ssl_ctx.add_certificate_authority(boost::asio::buffer(CA_PEM));
    ssl_ctx.set_verify_callback(boost::asio::ssl::host_name_verification("host.name"));
    asio::io_context ctx;
    tcp_ssl_connection conn(ctx, ssl_ctx);
    handshake_params params(integ_user, integ_passwd, integ_db);

    // Connect fails
    auto netres = conn.async_connect(get_tcp_endpoint(), params, as_netresult);
    netres.run();
    BOOST_TEST(netres.error().message().find("certificate verify failed") != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()  // ssl_certificate_validation

BOOST_AUTO_TEST_SUITE(ssl_mode_)

// All our CI servers support SSL, so enable should behave like required
BOOST_FIXTURE_TEST_CASE(any_enable, any_connection_fixture)
{
    // Setup
    auto params = default_connect_params(ssl_mode::enable);

    // Connect succeeds
    conn.async_connect(params, as_netresult).validate_no_error();
    BOOST_TEST(conn.uses_ssl());
}

// connection<>: all ssl modes work as disabled if the stream doesn't support ssl
BOOST_DATA_TEST_CASE(non_ssl_stream, data::make({ssl_mode::disable, ssl_mode::enable, ssl_mode::require}))
{
    // Setup
    asio::io_context ctx;
    tcp_connection conn(ctx);
    handshake_params params(integ_user, integ_passwd, integ_db);
    params.set_ssl(sample);

    // Physical connect
    conn.stream().connect(get_tcp_endpoint());

    // Handshake succeeds
    conn.async_handshake(params, as_netresult).validate_no_error();
    BOOST_TEST(!conn.uses_ssl());
}

// connection<>: disable can be used to effectively disable SSL
BOOST_AUTO_TEST_CASE(ssl_stream)
{
    struct
    {
        string_view name;
        ssl_mode mode;
        bool expect_ssl;
    } test_cases[] = {
        {"disable", ssl_mode::disable, false},
        {"enable",  ssl_mode::enable,  true },
        {"require", ssl_mode::require, true },
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            // Setup
            asio::io_context ctx;
            asio::ssl::context ssl_ctx(asio::ssl::context::tls_client);
            tcp_ssl_connection conn(ctx, ssl_ctx);
            handshake_params params(mysql_username, mysql_password);
            params.set_ssl(tc.mode);

            // Handshake succeeds
            conn.async_connect(get_tcp_endpoint(), params, as_netresult).validate_no_error();
            BOOST_TEST(conn.uses_ssl() == tc.expect_ssl);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

// Other handshake tests
BOOST_FIXTURE_TEST_CASE(no_database, any_connection_fixture)
{
    // Setup
    auto params = default_connect_params();
    params.database = "";

    // Connect succeeds
    conn.async_connect(params, as_netresult).validate_no_error();

    // No database selected
    results r;
    conn.async_execute("SELECT DATABASE()", r, as_netresult).validate_no_error();
    BOOST_TEST(r.rows() == makerows(1, nullptr), per_element());
}

BOOST_FIXTURE_TEST_CASE(bad_database, any_connection_fixture)
{
    // Setup
    auto params = default_connect_params();
    params.database = "bad_db";

    // Connect fails
    conn.async_connect(params, as_netresult)
        .validate_error(
            common_server_errc::er_dbaccess_denied_error,
            "Access denied for user 'integ_user'@'%' to database 'bad_db'"
        );
}

BOOST_TEST_DECORATOR(*run_if(&server_features::sha256))
BOOST_FIXTURE_TEST_CASE(unknown_auth_plugin, any_connection_fixture)
{
    // Note: sha256_password is not supported, so it's an unknown plugin to us
    // Setup
    auto params = default_connect_params(ssl_mode::require);
    params.username = "sha2p_user";
    params.password = "sha2p_password";

    // Connect fails
    conn.async_connect(params, as_netresult).validate_error(client_errc::unknown_auth_plugin);
}

BOOST_FIXTURE_TEST_CASE(bad_user, any_connection_fixture)
{
    // unreliable without SSL. If the default plugin requires SSL
    // (like SHA256), this would fail with 'ssl required'
    // Setup
    auto params = default_connect_params(ssl_mode::require);
    params.username = "non_existing_user";
    params.password = "bad_password";

    // Connect fails
    auto netres = conn.async_connect(params, as_netresult);
    netres.run();
    BOOST_TEST(netres.error().category().name() == get_common_server_category().name());
    BOOST_TEST(netres.error() != error_code());  // may be access denied or unknown auth plugin
}

BOOST_AUTO_TEST_SUITE_END()  // test_handshake

}  // namespace
