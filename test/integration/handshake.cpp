/*
 * handshake.cpp
 *
 *  Created on: Oct 22, 2019
 *      Author: ruben
 */

#include "mysql/connection.hpp"
#include <gtest/gtest.h>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace net = boost::asio;
using namespace testing;

using mysql::detail::make_error_code;

struct HandshakeTest : public Test
{
	mysql::connection_params connection_params {
		mysql::detail::CharacterSetLowerByte::utf8_general_ci,
		"root",
		"root",
		"awesome"
	};
	net::io_context ctx;
	mysql::connection<net::ip::tcp::socket> conn {ctx};
	mysql::error_code errc;

	HandshakeTest()
	{
		net::ip::tcp::endpoint endpoint (net::ip::address_v4::loopback(), 3306);
		conn.next_level().connect(endpoint);
	}

	~HandshakeTest()
	{
		conn.next_level().close(errc);
	}
};

TEST_F(HandshakeTest, SyncErrc_FastAuthSuccessfulLogin)
{
	conn.handshake(connection_params, errc);
	ASSERT_EQ(errc, mysql::error_code());
}

TEST_F(HandshakeTest, SyncErrc_FastAuthBadUser)
{
	connection_params.username = "non_existing_user";
	conn.handshake(connection_params, errc);
	ASSERT_EQ(errc, make_error_code(mysql::Error::access_denied_error));
}

TEST_F(HandshakeTest, SyncErrc_FastAuthBadPassword)
{
	connection_params.password = "bad_password";
	conn.handshake(connection_params, errc);
	ASSERT_EQ(errc, make_error_code(mysql::Error::access_denied_error));
}

TEST_F(HandshakeTest, SyncErrc_FastAuthBadDatabase)
{
	connection_params.database = "bad_database";
	conn.handshake(connection_params, errc);
	ASSERT_EQ(errc, make_error_code(mysql::Error::bad_db_error));
}


