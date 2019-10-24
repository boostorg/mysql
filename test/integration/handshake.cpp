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

TEST(IntegrationTest, Handshake_FastAuthSuccessfulLogin)
{
	mysql::connection_params params {
		mysql::detail::CharacterSetLowerByte::utf8_general_ci,
		"root",
		"root",
		"awesome"
	};

	net::io_context ctx;
	mysql::connection<net::ip::tcp::socket> conn (ctx);

	// TCP connect
	net::ip::tcp::endpoint endpoint (net::ip::address_v4::loopback(), 3306);
	conn.next_level().connect(endpoint);

	// Mysql handshake
	mysql::error_code errc;
	conn.handshake(params, errc);
	ASSERT_EQ(errc, mysql::error_code());
}
