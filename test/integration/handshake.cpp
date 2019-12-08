/*
 * handshake.cpp
 *
 *  Created on: Oct 22, 2019
 *      Author: ruben
 */

#include "mysql/connection.hpp"
#include "integration_test_common.hpp"
#include <boost/asio/use_future.hpp>

namespace net = boost::asio;
using namespace testing;

using mysql::detail::make_error_code;

namespace
{

struct HandshakeTest : public mysql::test::IntegTest
{
};

// Sync with error codes
TEST_F(HandshakeTest, SyncErrc_FastAuthSuccessfulLogin)
{
	conn.handshake(connection_params, errc);
	EXPECT_EQ(errc, mysql::error_code());
}

TEST_F(HandshakeTest, SyncErrc_FastAuthSuccessfulLoginEmptyPassword)
{
	connection_params.username = "empty_password_user";
	connection_params.password = "";
	conn.handshake(connection_params, errc);
	EXPECT_EQ(errc, mysql::error_code());
}

TEST_F(HandshakeTest, SyncErrc_FastAuthSuccessfulLoginNoDatabase)
{
	connection_params.database = "";
	conn.handshake(connection_params, errc);
	EXPECT_EQ(errc, mysql::error_code());
}

TEST_F(HandshakeTest, SyncErrc_FastAuthBadUser)
{
	connection_params.username = "non_existing_user";
	conn.handshake(connection_params, errc);
	EXPECT_EQ(errc, make_error_code(mysql::Error::access_denied_error));
}

TEST_F(HandshakeTest, SyncErrc_FastAuthBadPassword)
{
	connection_params.password = "bad_password";
	conn.handshake(connection_params, errc);
	EXPECT_EQ(errc, make_error_code(mysql::Error::access_denied_error));
}

TEST_F(HandshakeTest, SyncErrc_FastAuthBadDatabase)
{
	connection_params.database = "bad_database";
	conn.handshake(connection_params, errc);
	EXPECT_EQ(errc, make_error_code(mysql::Error::bad_db_error));
}

// Sync with exceptions
TEST_F(HandshakeTest, SyncExc_FastAuthSuccessfulLogin)
{
	EXPECT_NO_THROW(conn.handshake(connection_params));
}

TEST_F(HandshakeTest, SyncExc_FastAuthBadPassword)
{
	connection_params.password = "bad_password";
	EXPECT_THROW(conn.handshake(connection_params), boost::system::system_error);
}

// Async
TEST_F(HandshakeTest, Async_FastAuthSuccessfulLogin)
{
	auto fut = conn.async_handshake(connection_params, boost::asio::use_future);
	EXPECT_NO_THROW(fut.get());
}

TEST_F(HandshakeTest, Async_FastAuthSuccessfulLoginEmptyPassword)
{
	connection_params.username = "empty_password_user";
	connection_params.password = "";
	auto fut = conn.async_handshake(connection_params, boost::asio::use_future);
	EXPECT_NO_THROW(fut.get());
}

TEST_F(HandshakeTest, Async_FastAuthSuccessfulLoginNoDatabase)
{
	connection_params.database = "";
	auto fut = conn.async_handshake(connection_params, boost::asio::use_future);
	EXPECT_NO_THROW(fut.get());
}

TEST_F(HandshakeTest, Async_FastAuthBadUser)
{
	connection_params.username = "non_existing_user";
	auto fut = conn.async_handshake(connection_params, boost::asio::use_future);
	validate_future_exception(fut, make_error_code(mysql::Error::access_denied_error));
}

TEST_F(HandshakeTest, Async_FastAuthBadPassword)
{
	connection_params.password = "bad_password";
	auto fut = conn.async_handshake(connection_params, boost::asio::use_future);
	validate_future_exception(fut, make_error_code(mysql::Error::access_denied_error));
}

TEST_F(HandshakeTest, Async_FastAuthBadDatabase)
{
	connection_params.database = "bad_db";
	auto fut = conn.async_handshake(connection_params, boost::asio::use_future);
	validate_future_exception(fut, make_error_code(mysql::Error::bad_db_error));
}

} // anon namespace
