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
using mysql::error_code;
using mysql::error_info;
using mysql::Error;
using mysql::tcp_resultset;
using mysql::tcp_prepared_statement;

namespace
{

struct ExecuteStatementTest : public NetworkTest<> {};

// Iterator version
TEST_P(ExecuteStatementTest, Iterator_OkNoParams)
{
	std::forward_list<value> params;
	auto stmt = conn.prepare_statement("SELECT * FROM empty_table");
	auto result = GetParam()->execute_statement(stmt, params.begin(), params.end()); // execute
	result.validate_no_error();
	EXPECT_TRUE(result.value.valid());
}

TEST_P(ExecuteStatementTest, Iterator_OkWithParams)
{
	std::forward_list<value> params { value("item"), value(42) };
	auto stmt = conn.prepare_statement("SELECT * FROM empty_table WHERE id IN (?, ?)");
	auto result = GetParam()->execute_statement(stmt, params.begin(), params.end());
	result.validate_no_error();
	EXPECT_TRUE(result.value.valid());
}

TEST_P(ExecuteStatementTest, Iterator_MismatchedNumParams)
{
	std::forward_list<value> params { value("item") };
	auto stmt = conn.prepare_statement("SELECT * FROM empty_table WHERE id IN (?, ?)");
	auto result = GetParam()->execute_statement(stmt, params.begin(), params.end());
	result.validate_error(Error::wrong_num_params, {"param", "2", "1", "statement", "execute"});
	EXPECT_FALSE(result.value.valid());
}

// TODO: is there any way of making server return an error here?

// Container version
TEST_P(ExecuteStatementTest, Container_OkNoParams)
{
	auto stmt = conn.prepare_statement("SELECT * FROM empty_table");
	auto result = GetParam()->execute_statement(stmt, std::vector<value>()); // execute
	result.validate_no_error();
	EXPECT_TRUE(result.value.valid());
}

TEST_P(ExecuteStatementTest, Container_OkWithParams)
{
	std::vector<value> params { value("item"), value(42) };
	auto stmt = conn.prepare_statement("SELECT * FROM empty_table WHERE id IN (?, ?)");
	auto result = GetParam()->execute_statement(stmt, params);
	result.validate_no_error();
	EXPECT_TRUE(result.value.valid());
}

TEST_P(ExecuteStatementTest, Container_MismatchedNumParams)
{
	std::vector<value> params { value("item") };
	auto stmt = conn.prepare_statement("SELECT * FROM empty_table WHERE id IN (?, ?)");
	auto result = GetParam()->execute_statement(stmt, params);
	result.validate_error(Error::wrong_num_params, {"param", "2", "1", "statement", "execute"});
	EXPECT_FALSE(result.value.valid());
}

MYSQL_NETWORK_TEST_SUITE(ExecuteStatementTest);

// Other containers
struct ExecuteStatementOtherContainersTest : IntegTestAfterHandshake {};

TEST_F(ExecuteStatementOtherContainersTest, NoParams_CanUseNoStatementParamsVariable)
{
	auto stmt = conn.prepare_statement("SELECT * FROM empty_table");
	auto result = stmt.execute(mysql::no_statement_params);
	EXPECT_TRUE(result.valid());
}

TEST_F(ExecuteStatementOtherContainersTest, CArray)
{
	value arr [] = { value("hola"), value(10) };
	auto stmt = conn.prepare_statement("SELECT * FROM empty_table WHERE id IN (?, ?)");
	auto result = stmt.execute(arr);
	EXPECT_TRUE(result.valid());
}

}
