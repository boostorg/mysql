/*
 * prepare_statement.cpp
 *
 *  Created on: Feb 5, 2020
 *      Author: ruben
 */

#include "integration_test_common.hpp"

using namespace mysql::test;
using mysql::error_code;
using mysql::Error;

namespace
{

struct PrepareStatementTest : public IntegTestAfterHandshake
{
};

// sync errc
TEST_F(PrepareStatementTest, SyncErrc_OkNoParams)
{
	auto stmt = conn.prepare_statement("SELECT * FROM empty_table", errc, info);
	validate_no_error();
	ASSERT_TRUE(stmt.valid());
	EXPECT_GT(stmt.id(), 0);
	EXPECT_EQ(stmt.num_params(), 0);
}

TEST_F(PrepareStatementTest, SyncErrc_OkWithParams)
{
	auto stmt = conn.prepare_statement("SELECT * FROM empty_table WHERE id IN (?, ?)", errc, info);
	validate_no_error();
	ASSERT_TRUE(stmt.valid());
	EXPECT_GT(stmt.id(), 0);
	EXPECT_EQ(stmt.num_params(), 2);
}

TEST_F(PrepareStatementTest, SyncErrc_Error)
{
	auto stmt = conn.prepare_statement("SELECT * FROM bad_table WHERE id IN (?, ?)", errc, info);
	validate_sync_fail(Error::no_such_table, {"table", "doesn't exist", "bad_table"});
	EXPECT_FALSE(stmt.valid());
}

// sync exc
TEST_F(PrepareStatementTest, SyncExc_Ok)
{
	auto stmt = conn.prepare_statement("SELECT * FROM empty_table WHERE id = ?");
	ASSERT_TRUE(stmt.valid());
	EXPECT_GT(stmt.id(), 0);
	EXPECT_EQ(stmt.num_params(), 1);
}

TEST_F(PrepareStatementTest, SyncExc_Err)
{
	validate_sync_fail([this] {
		conn.prepare_statement("SELECT * FROM bad_table WHERE id IN (?, ?)");
	}, Error::no_such_table, {"table", "doesn't exist", "bad_table"});
}


// resultset::fetch_xxxx: repeat the same tests as in query
//    Cover no params, with params, select, insert, update, delete, with table/fields as params
// statements life cycle
//    Test select, insert, update, delete
//    Test with tables/fields as params
//    Test several executions
//    Test out of order execution

}


