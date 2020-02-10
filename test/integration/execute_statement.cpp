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

// Iterator version
using listit = std::forward_list<value>::const_iterator;
struct ExecuteStatementIteratorTraits
{
	static tcp_resultset sync_errc(const tcp_prepared_statement& stmt, listit first, listit last,
			error_code& errc, error_info& info)
	{
		return stmt.execute(first, last, errc, info);
	}

	static tcp_resultset sync_exc(const tcp_prepared_statement& stmt, listit first, listit last)
	{
		return stmt.execute(first, last);
	}

	template <typename CompletionToken>
	static auto async(const tcp_prepared_statement& stmt, listit first, listit last, CompletionToken&& token)
	{
		return stmt.async_execute(first, last, std::forward<CompletionToken>(token));
	}
};

struct ExecuteStatementIteratorTest : public NetworkTest<ExecuteStatementIteratorTraits> {};

TEST_P(ExecuteStatementIteratorTest, OkNoParams)
{
	std::forward_list<value> params;
	auto stmt = conn.prepare_statement("SELECT * FROM empty_table");
	auto result = GetParam().fun(stmt, params.begin(), params.end()); // execute
	result.validate_no_error();
	EXPECT_TRUE(result.value.valid());
}

TEST_P(ExecuteStatementIteratorTest, OkWithParams)
{
	std::forward_list<value> params { value("item"), value(42) };
	auto stmt = conn.prepare_statement("SELECT * FROM empty_table WHERE id IN (?, ?)");
	auto result = GetParam().fun(stmt, params.begin(), params.end());
	result.validate_no_error();
	EXPECT_TRUE(result.value.valid());
}

TEST_P(ExecuteStatementIteratorTest, MismatchedNumParams)
{
	std::forward_list<value> params { value("item") };
	auto stmt = conn.prepare_statement("SELECT * FROM empty_table WHERE id IN (?, ?)");
	auto result = GetParam().fun(stmt, params.begin(), params.end());
	result.validate_error(Error::wrong_num_params, {"param", "2", "1", "statement", "execute"});
	EXPECT_FALSE(result.value.valid());
}

// TODO: is there any way of making server return an error here?

MYSQL_NETWORK_TEST_SUITE(ExecuteStatementIteratorTest);

// Container version
struct ExecuteStatementContainerTraits
{
	static tcp_resultset sync_errc(const tcp_prepared_statement& stmt, const std::vector<value>& v,
			error_code& errc, error_info& info)
	{
		return stmt.execute(v, errc, info);
	}

	static tcp_resultset sync_exc(const tcp_prepared_statement& stmt, const std::vector<value>& v)
	{
		return stmt.execute(v);
	}

	template <typename CompletionToken>
	static auto async(const tcp_prepared_statement& stmt, const std::vector<value>& v, CompletionToken&& token)
	{
		return stmt.async_execute(v, std::forward<CompletionToken>(token));
	}
};

struct ExecuteStatementContainerTest : public NetworkTest<ExecuteStatementContainerTraits> {};

TEST_P(ExecuteStatementContainerTest, OkNoParams)
{
	auto stmt = conn.prepare_statement("SELECT * FROM empty_table");
	auto result = GetParam().fun(stmt, std::vector<value>()); // execute
	result.validate_no_error();
	EXPECT_TRUE(result.value.valid());
}

TEST_P(ExecuteStatementContainerTest, OkWithParams)
{
	std::vector<value> params { value("item"), value(42) };
	auto stmt = conn.prepare_statement("SELECT * FROM empty_table WHERE id IN (?, ?)");
	auto result = GetParam().fun(stmt, params);
	result.validate_no_error();
	EXPECT_TRUE(result.value.valid());
}

TEST_P(ExecuteStatementContainerTest, MismatchedNumParams)
{
	std::vector<value> params { value("item") };
	auto stmt = conn.prepare_statement("SELECT * FROM empty_table WHERE id IN (?, ?)");
	auto result = GetParam().fun(stmt, params);
	result.validate_error(Error::wrong_num_params, {"param", "2", "1", "statement", "execute"});
	EXPECT_FALSE(result.value.valid());
}

MYSQL_NETWORK_TEST_SUITE(ExecuteStatementContainerTest);

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
