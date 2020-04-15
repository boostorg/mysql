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

// Tests containing 'RequiresSha256' in their name require SHA256 functionality.
// This is used by a calling script to exclude these tests on systems
// that do not support this functionality

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
template <typename Stream>
struct SslIndifferentHandshakeTest :
	NetworkTest<Stream, network_testcase_with_ssl<Stream>, false>
{
	auto do_handshake()
	{
		this->connection_params.set_ssl(boost::mysql::ssl_options(this->GetParam().ssl));
		return this->GetParam().net->handshake(this->conn, this->connection_params);
	}

	// does handshake and verifies it went OK
	void do_handshake_ok()
	{
		auto result = do_handshake();
		result.validate_no_error();
		this->validate_ssl(this->GetParam().ssl);
	}
};

template <typename Stream>
struct SslSensitiveHandshakeTest : NetworkTest<Stream, network_testcase<Stream>, false>
{
	void set_ssl(ssl_mode m)
	{
		this->connection_params.set_ssl(boost::mysql::ssl_options(m));
	}

	auto do_handshake()
	{
		return this->GetParam().net->handshake(this->conn, this->connection_params);
	}

	void do_handshake_ok(ssl_mode m)
	{
		set_ssl(m);
		auto result = do_handshake();
		result.validate_no_error();
		this->validate_ssl(m);
	}
};

// mysql_native_password
template <typename Stream>
struct MysqlNativePasswordHandshakeTest : SslIndifferentHandshakeTest<Stream>
{
	void RegularUser_SuccessfulLogin()
	{
		this->set_credentials("mysqlnp_user", "mysqlnp_password");
		this->do_handshake_ok();
	}

	void EmptyPassword_SuccessfulLogin()
	{
		this->set_credentials("mysqlnp_empty_password_user", "");
		this->do_handshake_ok();
	}

	void BadPassword_FailedLogin()
	{
		this->set_credentials("mysqlnp_user", "bad_password");
		auto result = this->do_handshake();
		result.validate_error(errc::access_denied_error, {"access denied", "mysqlnp_user"});
	}
};

BOOST_MYSQL_NETWORK_TEST_SUITE(MysqlNativePasswordHandshakeTest)

BOOST_MYSQL_NETWORK_TEST(MysqlNativePasswordHandshakeTest, RegularUser_SuccessfulLogin)
BOOST_MYSQL_NETWORK_TEST(MysqlNativePasswordHandshakeTest, EmptyPassword_SuccessfulLogin)
BOOST_MYSQL_NETWORK_TEST(MysqlNativePasswordHandshakeTest, BadPassword_FailedLogin)


// Other handshake tests not depending on SSL mode
template <typename Stream>
struct MiscSslIndifferentHandshakeTest : SslIndifferentHandshakeTest<Stream>
{
	void NoDatabase_SuccessfulLogin()
	{
		this->connection_params.set_database("");
		this->do_handshake_ok();
	}

	void BadDatabase_FailedLogin()
	{
		this->connection_params.set_database("bad_database");
		auto result = this->do_handshake();
		result.validate_error(errc::dbaccess_denied_error, {"database", "bad_database"});
	}

	void UnknownAuthPlugin_FailedLogin_RequiresSha256()
	{
		// Note: sha256_password is not supported, so it's an unknown plugin to us
		this->set_credentials("sha2p_user", "sha2p_password");
		auto result = this->do_handshake();
		result.validate_error(errc::unknown_auth_plugin, {});
	}
};

BOOST_MYSQL_NETWORK_TEST_SUITE(MiscSslIndifferentHandshakeTest)

BOOST_MYSQL_NETWORK_TEST(MiscSslIndifferentHandshakeTest, NoDatabase_SuccessfulLogin)
BOOST_MYSQL_NETWORK_TEST(MiscSslIndifferentHandshakeTest, BadDatabase_FailedLogin)
BOOST_MYSQL_NETWORK_TEST(MiscSslIndifferentHandshakeTest, UnknownAuthPlugin_FailedLogin_RequiresSha256)

// caching_sha2_password
template <typename Stream>
struct CachingSha2HandshakeTest : SslSensitiveHandshakeTest<Stream>
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

	// Actual tests
	void SslOnCacheHit_SuccessfulLogin_RequiresSha256()
	{
		this->set_credentials("csha2p_user", "csha2p_password");
		load_sha256_cache("csha2p_user", "csha2p_password");
		this->do_handshake_ok(ssl_mode::require);
	}

	void SslOffCacheHit_SuccessfulLogin_RequiresSha256()
	{
		// As we are sending password hashed, it is OK to not have SSL for this
		this->set_credentials("csha2p_user", "csha2p_password");
		load_sha256_cache("csha2p_user", "csha2p_password");
		this->do_handshake_ok(ssl_mode::disable);
	}

	void SslOnCacheMiss_SuccessfulLogin_RequiresSha256()
	{
		this->set_credentials("csha2p_user", "csha2p_password");
		clear_sha256_cache();
		this->do_handshake_ok(ssl_mode::require);
	}

	void SslOffCacheMiss_FailedLogin_RequiresSha256()
	{
		// A cache miss would force us send a plaintext password over
		// a non-TLS connection, so we fail
		this->set_ssl(ssl_mode::disable);
		this->set_credentials("csha2p_user", "csha2p_password");
		clear_sha256_cache();
		auto result = this->do_handshake();
		result.validate_error(errc::auth_plugin_requires_ssl, {});
	}

	void EmptyPasswordSslOnCacheHit_SuccessfulLogin_RequiresSha256()
	{
		this->set_credentials("csha2p_empty_password_user", "");
		load_sha256_cache("csha2p_empty_password_user", "");
		this->do_handshake_ok(ssl_mode::require);
	}

	void EmptyPasswordSslOffCacheHit_SuccessfulLogin_RequiresSha256()
	{
		// Empty passwords are allowed over non-TLS connections
		this->set_credentials("csha2p_empty_password_user", "");
		load_sha256_cache("csha2p_empty_password_user", "");
		this->do_handshake_ok(ssl_mode::disable);
	}

	void EmptyPasswordSslOnCacheMiss_SuccessfulLogin_RequiresSha256()
	{
		this->set_credentials("csha2p_empty_password_user", "");
		clear_sha256_cache();
		this->do_handshake_ok(ssl_mode::require);
	}

	void EmptyPasswordSslOffCacheMiss_SuccessfulLogin_RequiresSha256()
	{
		// Empty passwords are allowed over non-TLS connections
		this->set_credentials("csha2p_empty_password_user", "");
		clear_sha256_cache();
		this->do_handshake_ok(ssl_mode::disable);
	}

	void BadPasswordSslOnCacheMiss_FailedLogin_RequiresSha256()
	{
		this->set_ssl(ssl_mode::require); // Note: test over non-TLS would return "ssl required"
		clear_sha256_cache();
		this->set_credentials("csha2p_user", "bad_password");
		auto result = this->do_handshake();
		result.validate_error(errc::access_denied_error, {"access denied", "csha2p_user"});
	}

	void BadPasswordSslOnCacheHit_FailedLogin_RequiresSha256()
	{
		this->set_ssl(ssl_mode::require); // Note: test over non-TLS would return "ssl required"
		this->set_credentials("csha2p_user", "bad_password");
		load_sha256_cache("csha2p_user", "csha2p_password");
		auto result = this->do_handshake();
		result.validate_error(errc::access_denied_error, {"access denied", "csha2p_user"});
	}
};

BOOST_MYSQL_NETWORK_TEST_SUITE(CachingSha2HandshakeTest)

BOOST_MYSQL_NETWORK_TEST(CachingSha2HandshakeTest, SslOnCacheHit_SuccessfulLogin_RequiresSha256)
BOOST_MYSQL_NETWORK_TEST(CachingSha2HandshakeTest, SslOffCacheHit_SuccessfulLogin_RequiresSha256)
BOOST_MYSQL_NETWORK_TEST(CachingSha2HandshakeTest, SslOnCacheMiss_SuccessfulLogin_RequiresSha256)
BOOST_MYSQL_NETWORK_TEST(CachingSha2HandshakeTest, SslOffCacheMiss_FailedLogin_RequiresSha256)
BOOST_MYSQL_NETWORK_TEST(CachingSha2HandshakeTest, EmptyPasswordSslOnCacheHit_SuccessfulLogin_RequiresSha256)
BOOST_MYSQL_NETWORK_TEST(CachingSha2HandshakeTest, EmptyPasswordSslOffCacheHit_SuccessfulLogin_RequiresSha256)
BOOST_MYSQL_NETWORK_TEST(CachingSha2HandshakeTest, EmptyPasswordSslOnCacheMiss_SuccessfulLogin_RequiresSha256)
BOOST_MYSQL_NETWORK_TEST(CachingSha2HandshakeTest, EmptyPasswordSslOffCacheMiss_SuccessfulLogin_RequiresSha256)
BOOST_MYSQL_NETWORK_TEST(CachingSha2HandshakeTest, BadPasswordSslOnCacheMiss_FailedLogin_RequiresSha256)
BOOST_MYSQL_NETWORK_TEST(CachingSha2HandshakeTest, BadPasswordSslOnCacheHit_FailedLogin_RequiresSha256)

// Misc tests that are sensitive on SSL
template <typename Stream>
struct MiscSslSensitiveHandshakeTest : SslSensitiveHandshakeTest<Stream>
{
	void BadUser_FailedLogin()
	{
		// unreliable without SSL. If the default plugin requires SSL
		// (like SHA256), this would fail with 'ssl required'
		this->set_ssl(ssl_mode::require);
		this->set_credentials("non_existing_user", "bad_password");
		auto result = this->do_handshake();
		result.validate_error(errc::access_denied_error, {"access denied", "non_existing_user"});
	}

	void SslEnable_SuccessfulLogin()
	{
		// In all our CI systems, our servers support SSL, so
		// ssl_mode::enable will do the same as ssl_mode::require.
		// We test for this fact.
		this->do_handshake_ok(ssl_mode::enable);
	}
};

BOOST_MYSQL_NETWORK_TEST_SUITE(MiscSslSensitiveHandshakeTest)

BOOST_MYSQL_NETWORK_TEST(MiscSslSensitiveHandshakeTest, BadUser_FailedLogin)
BOOST_MYSQL_NETWORK_TEST(MiscSslSensitiveHandshakeTest, SslEnable_SuccessfulLogin)

} // anon namespace
