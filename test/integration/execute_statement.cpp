/*
 * execute_statement.cpp
 *
 *  Created on: Feb 5, 2020
 *      Author: ruben
 */

#include "integration_test_common.hpp"
#include <forward_list>

using namespace boost::mysql::test;
using boost::mysql::value;
using boost::mysql::error_code;
using boost::mysql::error_info;
using boost::mysql::errc;
using boost::mysql::prepared_statement;

namespace
{

template <typename Stream>
struct ExecuteStatementTest : public NetworkTest<Stream>
{
	prepared_statement<Stream> do_prepare(std::string_view stmt)
	{
		auto res = this->GetParam().net->prepare_statement(this->conn, stmt);
		res.validate_no_error();
		return std::move(res.value);
	}

	auto do_execute(prepared_statement<Stream>& stmt, value_list_it first, value_list_it last)
	{
		return this->GetParam().net->execute_statement(stmt, first, last);
	}

	auto do_execute(prepared_statement<Stream>& stmt, const std::vector<value>& params)
	{
		return this->GetParam().net->execute_statement(stmt, params);
	}

	// Iterator version
	void Iterator_OkNoParams()
	{
		std::forward_list<value> params;
		auto stmt = do_prepare("SELECT * FROM empty_table");
		auto result = do_execute(stmt, params.begin(), params.end()); // execute
		result.validate_no_error();
		EXPECT_TRUE(result.value.valid());
	}

	void Iterator_OkWithParams()
	{
		std::forward_list<value> params { value("item"), value(42) };
		auto stmt = do_prepare("SELECT * FROM empty_table WHERE id IN (?, ?)");
		auto result = do_execute(stmt, params.begin(), params.end());
		result.validate_no_error();
		EXPECT_TRUE(result.value.valid());
	}

	void Iterator_MismatchedNumParams()
	{
		std::forward_list<value> params { value("item") };
		auto stmt = do_prepare("SELECT * FROM empty_table WHERE id IN (?, ?)");
		auto result = do_execute(stmt, params.begin(), params.end());
		result.validate_error(errc::wrong_num_params, {"param", "2", "1", "statement", "execute"});
		EXPECT_FALSE(result.value.valid());
	}

	void Iterator_ServerError()
	{
		std::forward_list<value> params { value("f0"), value("bad_date") };
		auto stmt = do_prepare("INSERT INTO inserts_table (field_varchar, field_date) VALUES (?, ?)");
		auto result = do_execute(stmt, params.begin(), params.end());
		result.validate_error(errc::truncated_wrong_value, {"field_date", "bad_date", "incorrect date value"});
		EXPECT_FALSE(result.value.valid());
	}

	// Container version
	void Container_OkNoParams()
	{
		auto stmt = do_prepare("SELECT * FROM empty_table");
		auto result = do_execute(stmt, std::vector<value>()); // execute
		result.validate_no_error();
		EXPECT_TRUE(result.value.valid());
	}

	void Container_OkWithParams()
	{
		std::vector<value> params { value("item"), value(42) };
		auto stmt = do_prepare("SELECT * FROM empty_table WHERE id IN (?, ?)");
		auto result = do_execute(stmt, params);
		result.validate_no_error();
		EXPECT_TRUE(result.value.valid());
	}

	void Container_MismatchedNumParams()
	{
		std::vector<value> params { value("item") };
		auto stmt = do_prepare("SELECT * FROM empty_table WHERE id IN (?, ?)");
		auto result = do_execute(stmt, params);
		result.validate_error(errc::wrong_num_params, {"param", "2", "1", "statement", "execute"});
		EXPECT_FALSE(result.value.valid());
	}

	void Container_ServerError()
	{
		auto stmt = do_prepare("INSERT INTO inserts_table (field_varchar, field_date) VALUES (?, ?)");
		auto result = do_execute(stmt, makevalues("f0", "bad_date"));
		result.validate_error(errc::truncated_wrong_value, {"field_date", "bad_date", "incorrect date value"});
		EXPECT_FALSE(result.value.valid());
	}
};

BOOST_MYSQL_NETWORK_TEST_SUITE(ExecuteStatementTest)

BOOST_MYSQL_NETWORK_TEST(ExecuteStatementTest, Iterator_OkNoParams)
BOOST_MYSQL_NETWORK_TEST(ExecuteStatementTest, Iterator_OkWithParams)
BOOST_MYSQL_NETWORK_TEST(ExecuteStatementTest, Iterator_MismatchedNumParams)
BOOST_MYSQL_NETWORK_TEST(ExecuteStatementTest, Iterator_ServerError)
BOOST_MYSQL_NETWORK_TEST(ExecuteStatementTest, Container_OkNoParams)
BOOST_MYSQL_NETWORK_TEST(ExecuteStatementTest, Container_OkWithParams)
BOOST_MYSQL_NETWORK_TEST(ExecuteStatementTest, Container_MismatchedNumParams)
BOOST_MYSQL_NETWORK_TEST(ExecuteStatementTest, Container_ServerError)



// Other containers
struct ExecuteStatementOtherContainersTest : IntegTest<boost::asio::ip::tcp::socket>
{
	ExecuteStatementOtherContainersTest() { handshake(boost::mysql::ssl_mode::disable); }
};

TEST_F(ExecuteStatementOtherContainersTest, NoParams_CanUseNoStatementParamsVariable)
{
	auto stmt = conn.prepare_statement("SELECT * FROM empty_table");
	auto result = stmt.execute(boost::mysql::no_statement_params);
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
