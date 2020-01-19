/*
 * handshake.cpp
 *
 *  Created on: Oct 22, 2019
 *      Author: ruben
 */

#include "mysql/connection.hpp"
#include "integration_test_common.hpp"
#include "test_common.hpp"
#include <boost/asio/use_future.hpp>

namespace net = boost::asio;
using namespace testing;
using namespace mysql::test;

using mysql::detail::make_error_code;
using mysql::error_info;
using mysql::Error;

namespace
{

struct HandshakeTest : public mysql::test::IntegTest
{
};

// Sync with error codes
TEST_F(HandshakeTest, SyncErrc_FastAuthSuccessfulLogin)
{
	conn.handshake(connection_params, errc, info);
	EXPECT_EQ(errc, mysql::error_code());
	EXPECT_EQ(info, error_info());
}

// TODO: review failure in Mac
/*TEST_F(HandshakeTest, SyncErrc_FastAuthSuccessfulLoginEmptyPassword)
{
	connection_params.username = "empty_password_user";
	connection_params.password = "";
	conn.handshake(connection_params, errc);
	EXPECT_EQ(errc, mysql::error_code());
}*/

TEST_F(HandshakeTest, SyncErrc_FastAuthSuccessfulLoginNoDatabase)
{
	connection_params.database = "";
	conn.handshake(connection_params, errc, info);
	EXPECT_EQ(errc, mysql::error_code());
	EXPECT_EQ(info, error_info());
}

TEST_F(HandshakeTest, SyncErrc_FastAuthBadUser)
{
	connection_params.username = "non_existing_user";
	conn.handshake(connection_params, errc, info);
	EXPECT_NE(errc, mysql::error_code());
	// TODO: if default auth plugin is unknown, unknown auth plugin is returned instead of access denied
	// EXPECT_EQ(errc, make_error_code(mysql::Error::access_denied_error));
}

TEST_F(HandshakeTest, SyncErrc_FastAuthBadPassword)
{
	connection_params.password = "bad_password";
	conn.handshake(connection_params, errc, info);
	EXPECT_EQ(errc, make_error_code(mysql::Error::access_denied_error));
	validate_error_info(info, {"access denied", "integ_user"});
}

TEST_F(HandshakeTest, SyncErrc_FastAuthBadDatabase)
{
	connection_params.database = "bad_database";
	conn.handshake(connection_params, errc, info);
	EXPECT_EQ(errc, make_error_code(mysql::Error::bad_db_error));
	validate_error_info(info, {"unknown database", "bad_database"});
}

// Sync with exceptions
TEST_F(HandshakeTest, SyncExc_FastAuthSuccessfulLogin)
{
	EXPECT_NO_THROW(conn.handshake(connection_params));
}

TEST_F(HandshakeTest, SyncExc_FastAuthBadPassword)
{
	connection_params.password = "bad_password";
	validate_sync_fail([this] {
		conn.handshake(connection_params);
	}, Error::access_denied_error, {"access denied", "integ_user"});
}

// Async
TEST_F(HandshakeTest, Async_FastAuthSuccessfulLogin)
{
	auto fut = conn.async_handshake(connection_params, boost::asio::use_future);
	EXPECT_EQ(fut.get(), error_info());
}

// TODO: review failure in Mac
/*TEST_F(HandshakeTest, Async_FastAuthSuccessfulLoginEmptyPassword)
{
	connection_params.username = "empty_password_user";
	connection_params.password = "";
	auto fut = conn.async_handshake(connection_params, boost::asio::use_future);
	EXPECT_EQ(fut.get(), error_info());
}*/

TEST_F(HandshakeTest, Async_FastAuthSuccessfulLoginNoDatabase)
{
	connection_params.database = "";
	auto fut = conn.async_handshake(connection_params, boost::asio::use_future);
	EXPECT_EQ(fut.get(), error_info());
}

TEST_F(HandshakeTest, Async_FastAuthBadUser)
{
	connection_params.username = "non_existing_user";
	auto fut = conn.async_handshake(connection_params, boost::asio::use_future);
	EXPECT_THROW(fut.get(), boost::system::system_error);
	// TODO: if default auth plugin is unknown, unknown auth plugin is returned instead of access denied
	// validate_future_exception(fut, make_error_code(mysql::Error::access_denied_error));
}

TEST_F(HandshakeTest, Async_FastAuthBadPassword)
{
	connection_params.password = "bad_password";
	validate_async_fail([&](auto&& cb) {
		conn.async_handshake(connection_params, std::move(cb));
	}, mysql::Error::access_denied_error, {"access denied", "integ_user"});
}

TEST_F(HandshakeTest, Async_FastAuthBadDatabase)
{
	connection_params.database = "bad_db";
	validate_async_fail([&](auto&& cb) {
		conn.async_handshake(connection_params, std::move(cb));
	}, mysql::Error::bad_db_error, {"unknown database", "bad_db"});
}

} // anon namespace
