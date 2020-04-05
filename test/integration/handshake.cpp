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

namespace net = boost::asio;
using namespace testing;
using namespace boost::mysql::test;

using boost::mysql::detail::make_error_code;
using boost::mysql::error_info;
using boost::mysql::errc;
using boost::mysql::error_code;

namespace
{

struct HandshakeTest : public IntegTest,
					   public testing::WithParamInterface<network_testcase>
{
	auto do_handshake()
	{
		connection_params.set_ssl(boost::mysql::ssl_options(GetParam().ssl));
		return GetParam().net->handshake(conn, connection_params);
	}
};

TEST_P(HandshakeTest, SuccessfulLogin)
{
	auto result = do_handshake();
	result.validate_no_error();
}

TEST_P(HandshakeTest, SuccessfulLoginEmptyPassword)
{
	connection_params.set_username("empty_password_user");
	connection_params.set_password("");
	auto result = do_handshake();
	result.validate_no_error();
}

TEST_P(HandshakeTest, SuccessfulLoginNoDatabase)
{
	connection_params.set_database("");
	auto result = do_handshake();
	result.validate_no_error();
}

TEST_P(HandshakeTest, BadUser)
{
	connection_params.set_username("non_existing_user");
	auto result = do_handshake();
	EXPECT_NE(result.err, error_code());
	// TODO: if default auth plugin is unknown, unknown auth plugin is returned instead of access denied
	// EXPECT_EQ(errc, make_error_code(mysql::errc::access_denied_error));
}

TEST_P(HandshakeTest, BadPassword)
{
	connection_params.set_password("bad_password");
	auto result = do_handshake();
	result.validate_error(errc::access_denied_error, {"access denied", "integ_user"});
}

TEST_P(HandshakeTest, BadDatabase)
{
	connection_params.set_database("bad_database");
	auto result = do_handshake();
	result.validate_error(errc::dbaccess_denied_error, {"database", "bad_database"});
}

MYSQL_NETWORK_TEST_SUITE(HandshakeTest);

} // anon namespace
