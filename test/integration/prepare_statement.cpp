/*
 * prepare_statement.cpp
 *
 *  Created on: Feb 5, 2020
 *      Author: ruben
 */

#include "integration_test_common.hpp"

using namespace boost::mysql::test;
using boost::mysql::error_code;
using boost::mysql::error_info;
using boost::mysql::errc;
using boost::mysql::tcp_prepared_statement;
using boost::mysql::tcp_connection;

namespace
{

struct PrepareStatementTest : public NetworkTest<>
{
};

TEST_P(PrepareStatementTest, OkNoParams)
{
	auto stmt = GetParam()->prepare_statement(conn, "SELECT * FROM empty_table");
	stmt.validate_no_error();
	ASSERT_TRUE(stmt.value.valid());
	EXPECT_GT(stmt.value.id(), 0u);
	EXPECT_EQ(stmt.value.num_params(), 0);
}

TEST_P(PrepareStatementTest, OkWithParams)
{
	auto stmt = GetParam()->prepare_statement(conn, "SELECT * FROM empty_table WHERE id IN (?, ?)");
	stmt.validate_no_error();
	ASSERT_TRUE(stmt.value.valid());
	EXPECT_GT(stmt.value.id(), 0u);
	EXPECT_EQ(stmt.value.num_params(), 2);
}

TEST_P(PrepareStatementTest, Error)
{
	auto stmt = GetParam()->prepare_statement(conn, "SELECT * FROM bad_table WHERE id IN (?, ?)");
	stmt.validate_error(errc::no_such_table, {"table", "doesn't exist", "bad_table"});
	EXPECT_FALSE(stmt.value.valid());
}

MYSQL_NETWORK_TEST_SUITE(PrepareStatementTest);

}
