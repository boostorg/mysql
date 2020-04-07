/*
 * handshake.cpp
 *
 *  Created on: Oct 22, 2019
 *      Author: ruben
 */

#include "boost/mysql/connection.hpp"
#include "integration_test_common.hpp"
#include "test_common.hpp"
#include <boost/asio/use_future.hpp>
#include <cstdlib>

namespace net = boost::asio;
using namespace testing;
using namespace boost::mysql::test;

using boost::mysql::ssl_mode;
using boost::mysql::detail::make_error_code;
using boost::mysql::error_info;
using boost::mysql::errc;
using boost::mysql::error_code;
using boost::mysql::detail::stringize;

namespace
{

// Handshake tests not depending on whether we use SSL or not
struct SslIndifferentHandshakeTest : public IntegTest,
									 public testing::WithParamInterface<network_testcase>
{
	auto do_handshake()
	{
		connection_params.set_ssl(boost::mysql::ssl_options(GetParam().ssl));
		return GetParam().net->handshake(conn, connection_params);
	}

	// does handshake and verifies it went OK
	void do_handshake_ok()
	{
		auto result = do_handshake();
		result.validate_no_error();
		validate_ssl(GetParam().ssl);
	}
};

struct SslSensitiveHandshakeTest : public IntegTest,
								   public testing::WithParamInterface<network_functions*>
{
	void set_ssl(ssl_mode m)
	{
		connection_params.set_ssl(boost::mysql::ssl_options(m));
	}

	auto do_handshake()
	{
		return GetParam()->handshake(conn, connection_params);
	}

	void do_handshake_ok(ssl_mode m)
	{
		set_ssl(m);
		auto result = do_handshake();
		result.validate_no_error();
		validate_ssl(m);
	}
};

// mysql_native_password
struct MysqlNativePasswordHandshakeTest : SslIndifferentHandshakeTest {};

TEST_P(MysqlNativePasswordHandshakeTest, RegularUser_SuccessfulLogin)
{
	set_credentials("mysqlnp_user", "mysqlnp_password");
	do_handshake_ok();
}

TEST_P(MysqlNativePasswordHandshakeTest, EmptyPassword_SuccessfulLogin)
{
	set_credentials("mysqlnp_empty_password_user", "");
	do_handshake_ok();
}

TEST_P(MysqlNativePasswordHandshakeTest, BadPassword_FailedLogin)
{
	set_credentials("mysqlnp_user", "bad_password");
	auto result = do_handshake();
	result.validate_error(errc::access_denied_error, {"access denied", "mysqlnp_user"});
}

MYSQL_NETWORK_TEST_SUITE(MysqlNativePasswordHandshakeTest);

// Other handshake tests not depending on SSL mode
struct MiscSslIndifferentHandshakeTest : SslIndifferentHandshakeTest {};

TEST_P(MiscSslIndifferentHandshakeTest, NoDatabase_SuccessfulLogin)
{
	connection_params.set_database("");
	do_handshake_ok();
}

TEST_P(MiscSslIndifferentHandshakeTest, BadDatabase_FailedLogin)
{
	connection_params.set_database("bad_database");
	auto result = do_handshake();
	result.validate_error(errc::dbaccess_denied_error, {"database", "bad_database"});
}

TEST_P(MiscSslIndifferentHandshakeTest, UnknownAuthPlugin_FailedLogin_RequiresSha256)
{
	set_credentials("sha2p_user", "sha2p_password");
	auto result = do_handshake();
	result.validate_error(errc::unknown_auth_plugin, {});
}

MYSQL_NETWORK_TEST_SUITE(MiscSslIndifferentHandshakeTest);

// caching_sha2_password
struct CachingSha2HandshakeTest : SslSensitiveHandshakeTest
{
	void load_sha256_cache(const std::string& user, const std::string& password)
	{
		if (password.empty())
		{
			check_call(stringize("mysql -u ", user, " -e \"\""));
		}
		else
		{
			check_call(stringize("mysql -u ", user, " -p", password, " -e \"\""));
		}
	}

	void clear_sha256_cache()
	{
		check_call("mysql -u root -e \"FLUSH PRIVILEGES\"");
	}
};

// Tests containing 'RequiresSha256' in their name require SHA256 functionality.
// This is used by a calling script to exclude these tests on systems
// that do not support this functionality
TEST_P(CachingSha2HandshakeTest, SslOnCacheHit_SuccessfulLogin_RequiresSha256)
{
	set_credentials("csha2p_user", "csha2p_password");
	load_sha256_cache("csha2p_user", "csha2p_password");
	do_handshake_ok(ssl_mode::require);
}

TEST_P(CachingSha2HandshakeTest, SslOffCacheHit_SuccessfulLogin_RequiresSha256)
{
	// As we are sending password hashed, it is OK to not have SSL for this
	set_credentials("csha2p_user", "csha2p_password");
	load_sha256_cache("csha2p_user", "csha2p_password");
	do_handshake_ok(ssl_mode::disable);
}

TEST_P(CachingSha2HandshakeTest, SslOnCacheMiss_SuccessfulLogin_RequiresSha256)
{
	set_credentials("csha2p_user", "csha2p_password");
	clear_sha256_cache();
	do_handshake_ok(ssl_mode::require);
}

TEST_P(CachingSha2HandshakeTest, SslOffCacheMiss_FailedLogin_RequiresSha256)
{
	// A cache miss would force us send a plaintext password over
	// a non-TLS connection, so we fail
	set_ssl(ssl_mode::disable);
	set_credentials("csha2p_user", "csha2p_password");
	clear_sha256_cache();
	auto result = do_handshake();
	result.validate_error(errc::auth_plugin_requires_ssl, {});
}

TEST_P(CachingSha2HandshakeTest, EmptyPasswordSslOnCacheHit_SuccessfulLogin_RequiresSha256)
{
	set_credentials("csha2p_empty_password_user", "");
	load_sha256_cache("csha2p_empty_password_user", "");
	do_handshake_ok(ssl_mode::require);
}

TEST_P(CachingSha2HandshakeTest, EmptyPasswordSslOffCacheHit_SuccessfulLogin_RequiresSha256)
{
	// Empty passwords are allowed over non-TLS connections
	set_credentials("csha2p_empty_password_user", "");
	load_sha256_cache("csha2p_empty_password_user", "");
	do_handshake_ok(ssl_mode::disable);
}

TEST_P(CachingSha2HandshakeTest, EmptyPasswordSslOnCacheMiss_SuccessfulLogin_RequiresSha256)
{
	set_credentials("csha2p_empty_password_user", "");
	clear_sha256_cache();
	do_handshake_ok(ssl_mode::require);
}

TEST_P(CachingSha2HandshakeTest, EmptyPasswordSslOffCacheMiss_SuccessfulLogin_RequiresSha256)
{
	// Empty passwords are allowed over non-TLS connections
	set_credentials("csha2p_empty_password_user", "");
	clear_sha256_cache();
	do_handshake_ok(ssl_mode::disable);
}

TEST_P(CachingSha2HandshakeTest, BadPasswordSslOnCacheMiss_FailedLogin_RequiresSha256)
{
	set_ssl(ssl_mode::require); // Note: test over non-TLS would return "ssl required"
	clear_sha256_cache();
	set_credentials("csha2p_user", "bad_password");
	auto result = do_handshake();
	result.validate_error(errc::access_denied_error, {"access denied", "csha2p_user"});
}

TEST_P(CachingSha2HandshakeTest, BadPasswordSslOnCacheHit_FailedLogin_RequiresSha256)
{
	set_ssl(ssl_mode::require); // Note: test over non-TLS would return "ssl required"
	set_credentials("csha2p_user", "bad_password");
	load_sha256_cache("csha2p_user", "csha2p_password");
	auto result = do_handshake();
	result.validate_error(errc::access_denied_error, {"access denied", "csha2p_user"});
}

INSTANTIATE_TEST_SUITE_P(Default, CachingSha2HandshakeTest, testing::ValuesIn(
	all_network_functions
), [](const auto& param_info) { return param_info.param->name(); });

// Misc tests that are sensitive on SSL
struct MiscSslSensitiveHandshakeTest : SslSensitiveHandshakeTest {};

TEST_P(MiscSslSensitiveHandshakeTest, BadUser_FailedLogin)
{
	// unreliable without SSL. If the default plugin requires SSL
	// (like SHA256), this would fail with 'ssl required'
	set_ssl(ssl_mode::require);
	set_credentials("non_existing_user", "bad_password");
	auto result = do_handshake();
	result.validate_error(errc::access_denied_error, {"access denied", "non_existing_user"});
}

TEST_P(MiscSslSensitiveHandshakeTest, SslEnable_SuccessfulLogin)
{
	// In all our CI systems, our servers support SSL, so
	// ssl_mode::enable will do the same as ssl_mode::require.
	// We test for this fact.
	do_handshake_ok(ssl_mode::enable);
}

INSTANTIATE_TEST_SUITE_P(Default, MiscSslSensitiveHandshakeTest, testing::ValuesIn(
	all_network_functions
), [](const auto& param_info) { return param_info.param->name(); });


} // anon namespace
