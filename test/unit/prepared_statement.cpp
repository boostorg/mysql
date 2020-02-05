/*
 * prepared_statement.cpp
 *
 *  Created on: Feb 5, 2020
 *      Author: ruben
 */

#include "test_common.hpp"
#include "mysql/prepared_statement.hpp"
#include <boost/asio/ip/tcp.hpp>

using namespace mysql::detail;

namespace
{

using stream_type = boost::asio::ip::tcp::socket;
using channel_type = mysql::detail::channel<stream_type>;
using stmt_type = mysql::prepared_statement<stream_type>;

struct PreparedStatementTest : public testing::Test
{
	boost::asio::io_context ctx; // TODO: change these for a mock stream
	stream_type socket {ctx};
	channel_type channel {socket};
};

TEST_F(PreparedStatementTest, DefaultConstructor_Trivial_Invalid)
{
	stmt_type stmt;
	EXPECT_FALSE(stmt.valid());
}

TEST_F(PreparedStatementTest, InitializingConstructor_Trivial_Valid)
{

	stmt_type stmt (channel, com_stmt_prepare_ok_packet{int4(10), int2(9), int2(8), int2(7)});
	EXPECT_TRUE(stmt.valid());
	EXPECT_EQ(stmt.id(), 10);
	EXPECT_EQ(stmt.num_params(), 8);
}

TEST_F(PreparedStatementTest, MoveConstructor_FromDefaultConstructed_Invalid)
{
	stmt_type stmt {stmt_type()};
	EXPECT_FALSE(stmt.valid());
}

TEST_F(PreparedStatementTest, MoveConstructor_FromValid_Valid)
{

	stmt_type stmt (stmt_type(
		channel, com_stmt_prepare_ok_packet{int4(10), int2(9), int2(8), int2(7)}
	));
	EXPECT_TRUE(stmt.valid());
	EXPECT_EQ(stmt.id(), 10);
	EXPECT_EQ(stmt.num_params(), 8);
}

TEST_F(PreparedStatementTest, MoveAssignment_FromDefaultConstructed_Invalid)
{
	stmt_type stmt (channel, com_stmt_prepare_ok_packet{int4(10), int2(9), int2(8), int2(7)});
	stmt = stmt_type();
	EXPECT_FALSE(stmt.valid());
	stmt = stmt_type();
	EXPECT_FALSE(stmt.valid());
}

TEST_F(PreparedStatementTest, MoveAssignment_FromValid_Valid)
{

	stmt_type stmt;
	stmt = stmt_type (channel, com_stmt_prepare_ok_packet{int4(10), int2(9), int2(8), int2(7)});
	EXPECT_TRUE(stmt.valid());
	EXPECT_EQ(stmt.id(), 10);
	EXPECT_EQ(stmt.num_params(), 8);
	stmt = stmt_type (channel, com_stmt_prepare_ok_packet{int4(1), int2(2), int2(3), int2(4)});
	EXPECT_TRUE(stmt.valid());
	EXPECT_EQ(stmt.id(), 1);
	EXPECT_EQ(stmt.num_params(), 3);
}

}


