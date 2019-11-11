/*
 * query.cpp
 *
 *  Created on: Nov 11, 2019
 *      Author: ruben
 */

#include "mysql/connection.hpp"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/use_future.hpp>

namespace net = boost::asio;
using namespace testing;

using mysql::detail::make_error_code;

namespace
{

struct QueryTest : public Test
{
	using resultset_type = mysql::resultset<net::ip::tcp::socket>;

	net::io_context ctx;
	mysql::connection<net::ip::tcp::socket> conn {ctx};
	mysql::error_code errc;

	QueryTest()
	{
		mysql::connection_params connection_params {
			mysql::collation::utf8_general_ci,
			"root",
			"root",
			"awesome"
		};
		net::ip::tcp::endpoint endpoint (net::ip::address_v4::loopback(), 3306);
		conn.next_level().connect(endpoint);
		conn.handshake(connection_params);
	}

	~QueryTest()
	{
		conn.next_level().close(errc);
	}
};

TEST_F(QueryTest, SyncErrc_InsertQueryOk)
{
	auto result = conn.query(
			"INSERT INTO inserts_table (field_varchar, field_date) VALUES ('v0', '2010-10-11')", errc);
	ASSERT_EQ(errc, mysql::error_code());
	EXPECT_TRUE(result.complete());
	EXPECT_TRUE(result.fields().empty());
	EXPECT_EQ(result.affected_rows(), 1);
	EXPECT_EQ(result.warning_count(), 0);
	EXPECT_GT(result.last_insert_id(), 0);
	EXPECT_EQ(result.info(), "");
}

TEST_F(QueryTest, SyncErrc_UpdateQueryOk)
{
	auto result = conn.query(
			"UPDATE updates_table SET field_int = 50", errc);
	ASSERT_EQ(errc, mysql::error_code());
	EXPECT_TRUE(result.complete());
	EXPECT_TRUE(result.fields().empty());
	EXPECT_EQ(result.affected_rows(), 2);
	EXPECT_EQ(result.warning_count(), 0);
	EXPECT_EQ(result.last_insert_id(), 0);
	EXPECT_THAT(std::string(result.info()), HasSubstr("Rows matched"));
}


}





