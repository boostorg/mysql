//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "integration_test_common.hpp"

using namespace boost::mysql::test;
using boost::mysql::error_code;
using boost::mysql::error_info;
using boost::mysql::errc;
using boost::mysql::prepared_statement;
using boost::mysql::connection;

namespace
{

template <typename Stream>
struct PrepareStatementTest : public NetworkTest<Stream>
{
	auto do_prepare(std::string_view stmt)
	{
		return this->GetParam().net->prepare_statement(this->conn, stmt);
	}

	void OkNoParams()
	{
		auto stmt = do_prepare("SELECT * FROM empty_table");
		stmt.validate_no_error();
		ASSERT_TRUE(stmt.value.valid());
		EXPECT_GT(stmt.value.id(), 0u);
		EXPECT_EQ(stmt.value.num_params(), 0);
	}

	void OkWithParams()
	{
		auto stmt = do_prepare("SELECT * FROM empty_table WHERE id IN (?, ?)");
		stmt.validate_no_error();
		ASSERT_TRUE(stmt.value.valid());
		EXPECT_GT(stmt.value.id(), 0u);
		EXPECT_EQ(stmt.value.num_params(), 2);
	}

	void InvalidStatement()
	{
		auto stmt = do_prepare("SELECT * FROM bad_table WHERE id IN (?, ?)");
		stmt.validate_error(errc::no_such_table, {"table", "doesn't exist", "bad_table"});
		EXPECT_FALSE(stmt.value.valid());
	}
};

BOOST_MYSQL_NETWORK_TEST_SUITE(PrepareStatementTest)

BOOST_MYSQL_NETWORK_TEST(PrepareStatementTest, OkNoParams)
BOOST_MYSQL_NETWORK_TEST(PrepareStatementTest, OkWithParams)
BOOST_MYSQL_NETWORK_TEST(PrepareStatementTest, InvalidStatement)


}
