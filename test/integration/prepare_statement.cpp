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

struct PrepareStatementTraits
{
	static tcp_prepared_statement sync_errc(tcp_connection& conn, std::string_view statement,
			error_code& err, error_info& info)
	{
		return conn.prepare_statement(statement, err, info);
	}

	static tcp_prepared_statement sync_exc(tcp_connection& conn, std::string_view statement)
	{
		return conn.prepare_statement(statement);
	}

	template <typename CompletionToken>
	static auto async(tcp_connection& conn, std::string_view statement, CompletionToken&& token)
	{
		return conn.async_prepare_statement(statement, std::forward<CompletionToken>(token));
	}
};

struct PrepareStatementTest : public NetworkTest<PrepareStatementTraits>
{
};

// sync errc
TEST_P(PrepareStatementTest, OkNoParams)
{
	auto stmt = GetParam().fun(conn, "SELECT * FROM empty_table");
	stmt.validate_no_error();
	ASSERT_TRUE(stmt.value.valid());
	EXPECT_GT(stmt.value.id(), 0);
	EXPECT_EQ(stmt.value.num_params(), 0);
}

TEST_P(PrepareStatementTest, OkWithParams)
{
	auto stmt = GetParam().fun(conn, "SELECT * FROM empty_table WHERE id IN (?, ?)");
	stmt.validate_no_error();
	ASSERT_TRUE(stmt.value.valid());
	EXPECT_GT(stmt.value.id(), 0);
	EXPECT_EQ(stmt.value.num_params(), 2);
}

TEST_P(PrepareStatementTest, Error)
{
	auto stmt = GetParam().fun(conn, "SELECT * FROM bad_table WHERE id IN (?, ?)");
	stmt.validate_error(Error::no_such_table, {"table", "doesn't exist", "bad_table"});
	EXPECT_FALSE(stmt.value.valid());
}

MYSQL_NETWORK_TEST_SUITE(PrepareStatementTest);

// statements life cycle
//    Test select, insert, update, delete
//    Test with tables/fields as params
//    Test several executions
//    Test out of order execution

}


