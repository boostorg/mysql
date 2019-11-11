/*
 * query.cpp
 *
 *  Created on: Nov 11, 2019
 *      Author: ruben
 */

#include "mysql/connection.hpp"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/use_future.hpp>

namespace net = boost::asio;
using namespace testing;

using mysql::detail::make_error_code;

namespace
{

struct QueryTest : public Test
{
	using resultset_type = mysql::resultset<net::ip::tcp::socket>;

	net::io_context ctx;
	mysql::connection<net::ip::tcp::socket> conn {ctx};
	mysql::error_code errc;

	QueryTest()
	{
		mysql::connection_params connection_params {
			mysql::collation::utf8_general_ci,
			"root",
			"root",
			"awesome"
		};
		net::ip::tcp::endpoint endpoint (net::ip::address_v4::loopback(), 3306);
		conn.next_level().connect(endpoint);
		conn.handshake(connection_params);
	}

	~QueryTest()
	{
		conn.next_level().close(errc);
	}

	void ValidateEof(const resultset_type& result, int affected_rows=0,
			int warnings=0, int last_insert=0, std::string_view info="")
	{
		EXPECT_TRUE(result.valid());
		EXPECT_TRUE(result.complete());
		EXPECT_EQ(result.affected_rows(), affected_rows);
		EXPECT_EQ(result.warning_count(), warnings);
		EXPECT_EQ(result.last_insert_id(), last_insert);
		EXPECT_EQ(result.info(), info);
	}
};

template <typename... Types>
std::vector<mysql::value> makevalues(Types&&... args)
{
	return std::vector<mysql::value>{mysql::value(std::forward<Types>(args))...};
}

TEST_F(QueryTest, QuerySyncErrc_InsertQueryOk)
{
	auto result = conn.query(
			"INSERT INTO inserts_table (field_varchar, field_date) VALUES ('v0', '2010-10-11')", errc);
	ASSERT_EQ(errc, mysql::error_code());
	EXPECT_TRUE(result.fields().empty());
	EXPECT_TRUE(result.valid());
	EXPECT_TRUE(result.complete());
	EXPECT_EQ(result.affected_rows(), 1);
	EXPECT_EQ(result.warning_count(), 0);
	EXPECT_GT(result.last_insert_id(), 0);
	EXPECT_EQ(result.info(), "");
}

TEST_F(QueryTest, QuerySyncErrc_InsertQueryFailed)
{
	auto result = conn.query(
			"INSERT INTO bad_table (field_varchar, field_date) VALUES ('v0', '2010-10-11')", errc);
	ASSERT_EQ(errc, make_error_code(mysql::Error::no_such_table));
	EXPECT_FALSE(result.valid());
}

TEST_F(QueryTest, QuerySyncErrc_UpdateQueryOk)
{
	auto result = conn.query(
			"UPDATE updates_table SET field_int = field_int+1", errc);
	ASSERT_EQ(errc, mysql::error_code());
	EXPECT_TRUE(result.fields().empty());
	EXPECT_TRUE(result.valid());
	EXPECT_TRUE(result.complete());
	EXPECT_EQ(result.affected_rows(), 2);
	EXPECT_EQ(result.warning_count(), 0);
	EXPECT_EQ(result.last_insert_id(), 0);
	EXPECT_THAT(std::string(result.info()), HasSubstr("Rows matched"));
}

// Protocol handling of QUERY with results
TEST_F(QueryTest, QuerySyncErrc_SelectOk)
{
	auto result = conn.query("SELECT * FROM empty_table", errc);
	ASSERT_EQ(errc, mysql::error_code());
	EXPECT_TRUE(result.valid());
	EXPECT_FALSE(result.complete());
	EXPECT_EQ(result.fields().size(), 2);
	// TODO: validate these metadata
}

TEST_F(QueryTest, FetchOneSyncErrc_SelectOkNoResults)
{
	auto result = conn.query("SELECT * FROM empty_table");
	EXPECT_TRUE(result.valid());
	EXPECT_FALSE(result.complete());
	EXPECT_EQ(result.fields().size(), 2);

	// Already in the end of the resultset, we receive the EOF
	const mysql::row* row = result.fetch_one(errc);
	EXPECT_EQ(errc, mysql::error_code());
	EXPECT_EQ(row, nullptr);
	// TODO: validate metadata
	ValidateEof(result);

	// Fetching again just returns null
	row = result.fetch_one(errc);
	EXPECT_EQ(errc, mysql::error_code());
	EXPECT_EQ(row, nullptr);
	ValidateEof(result);
}

TEST_F(QueryTest, FetchOneSyncErrc_SelectOkOneRow)
{
	auto result = conn.query("SELECT * FROM one_row_table");
	EXPECT_TRUE(result.valid());
	EXPECT_FALSE(result.complete());
	EXPECT_EQ(result.fields().size(), 2);

	// Fetch only row
	const mysql::row* row = result.fetch_one(errc);
	ASSERT_EQ(errc, mysql::error_code());
	ASSERT_NE(row, nullptr);
	// TODO: validate metadata
	EXPECT_EQ(row->values(), makevalues(1, "f0"));
	EXPECT_FALSE(result.complete());

	// Fetch next: end of resultset
	row = result.fetch_one(errc);
	ASSERT_EQ(errc, mysql::error_code());
	ASSERT_EQ(row, nullptr);
	ValidateEof(result);
}

TEST_F(QueryTest, FetchOneSyncErrc_SelectOkTwoRows)
{
	auto result = conn.query("SELECT * FROM two_rows_table");
	EXPECT_TRUE(result.valid());
	EXPECT_FALSE(result.complete());
	EXPECT_EQ(result.fields().size(), 2);

	// Fetch first row
	const mysql::row* row = result.fetch_one(errc);
	ASSERT_EQ(errc, mysql::error_code());
	ASSERT_NE(row, nullptr);
	// TODO: validate metadata
	EXPECT_EQ(row->values(), makevalues(1, "f0"));
	EXPECT_FALSE(result.complete());

	// Fetch next row
	row = result.fetch_one(errc);
	ASSERT_EQ(errc, mysql::error_code());
	ASSERT_NE(row, nullptr);
	// TODO: validate metadata
	EXPECT_EQ(row->values(), makevalues(2, "f1"));
	EXPECT_FALSE(result.complete());

	// Fetch next: end of resultset
	row = result.fetch_one(errc);
	ASSERT_EQ(errc, mysql::error_code());
	ASSERT_EQ(row, nullptr);
	ValidateEof(result);
}

TEST_F(QueryTest, QuerySyncErrc_SelectQueryFailed)
{
	auto result = conn.query("SELECT field_varchar, field_bad FROM one_row_table", errc);
	ASSERT_EQ(errc, make_error_code(mysql::Error::bad_field_error));
	EXPECT_FALSE(result.valid());
}

// Query for INT types





}





