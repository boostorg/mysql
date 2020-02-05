/*
 * execute_statement.cpp
 *
 *  Created on: Feb 5, 2020
 *      Author: ruben
 */

#include "integration_test_common.hpp"
#include <forward_list>

using namespace mysql::test;
using mysql::value;
using mysql::Error;

namespace
{

struct ExecuteStatementTest : public IntegTestAfterHandshake
{
};

TEST_F(ExecuteStatementTest, IteratorsSyncErrc_OkNoParams)
{
	std::forward_list<value> params;
	auto stmt = conn.prepare_statement("SELECT * FROM empty_table");
	auto result = stmt.execute(params.begin(), params.end(), errc, info);
	validate_no_error();
	EXPECT_TRUE(result.valid());
}

TEST_F(ExecuteStatementTest, IteratorsSyncErrc_OkWithParams)
{
	std::forward_list<value> params { value("item"), value(42) };
	auto stmt = conn.prepare_statement("SELECT * FROM empty_table WHERE id IN (?, ?)");
	auto result = stmt.execute(params.begin(), params.end(), errc, info);
	validate_no_error();
	EXPECT_TRUE(result.valid());
}

TEST_F(ExecuteStatementTest, IteratorsSyncErrc_Error)
{
	std::forward_list<value> params { value("item") };
	auto stmt = conn.prepare_statement("SELECT * FROM empty_table WHERE id IN (?, ?)");
	auto result = stmt.execute(params.begin(), params.end(), errc, info);
	validate_sync_fail(Error::wrong_num_params, {"param", "2", "1", "statement", "execute"});
	EXPECT_FALSE(result.valid());
}

// prepared_statement::execute
//    OK, no params
//    OK, with params
//    OK, select, insert, update, delete
//    Error, wrong number of parameters
//    Collection version

}
