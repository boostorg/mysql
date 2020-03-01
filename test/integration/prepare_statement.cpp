/*
 * prepare_statement.cpp
 *
 *  Created on: Feb 5, 2020
 *      Author: ruben
 */

#include "integration_test_common.hpp"

using namespace mysql::test;
using mysql::error_code;
using mysql::error_info;
using mysql::Error;
using mysql::tcp_prepared_statement;
using mysql::tcp_connection;

namespace
{

struct PrepareStatementTest : public NetworkTest<>
{
};

// sync errc
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
	stmt.validate_error(Error::no_such_table, {"table", "doesn't exist", "bad_table"});
	EXPECT_FALSE(stmt.value.valid());
}

MYSQL_NETWORK_TEST_SUITE(PrepareStatementTest);

}
