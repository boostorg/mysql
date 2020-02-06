/*
 * query.cpp
 *
 *  Created on: Nov 11, 2019
 *      Author: ruben
 */

#include "mysql/connection.hpp"
#include <gmock/gmock.h> // for EXPECT_THAT()
#include <boost/asio/use_future.hpp>
#include "metadata_validator.hpp"
#include "integration_test_common.hpp"
#include "test_common.hpp"

namespace net = boost::asio;
using namespace testing;
using namespace mysql::test;
using mysql::detail::make_error_code;
using mysql::test::meta_validator;
using mysql::test::validate_meta;
using mysql::field_metadata;
using mysql::field_type;
using mysql::error_code;
using mysql::error_info;

namespace
{

struct QueryTest : public mysql::test::IntegTestAfterHandshake
{
	auto make_query_initiator(const char* sql)
	{
		return [this, sql](auto&& cb) {
			conn.async_query(sql, cb);
		};
	}
};

// Query, sync errc
TEST_F(QueryTest, QuerySyncErrc_InsertQueryOk)
{
	const char* sql = "INSERT INTO inserts_table (field_varchar, field_date) VALUES ('v0', '2010-10-11')";
	auto result = conn.query(sql, errc, info);
	validate_no_error();
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
	const char* sql = "INSERT INTO bad_table (field_varchar, field_date) VALUES ('v0', '2010-10-11')";
	auto result = conn.query(sql, errc, info);
	validate_sync_fail(mysql::Error::no_such_table, {"table", "doesn't exist", "bad_table"});
	EXPECT_FALSE(result.valid());
}

TEST_F(QueryTest, QuerySyncErrc_UpdateQueryOk)
{
	const char* sql = "UPDATE updates_table SET field_int = field_int+1";
	auto result = conn.query(sql, errc, info);
	validate_no_error();
	EXPECT_TRUE(result.fields().empty());
	EXPECT_TRUE(result.valid());
	EXPECT_TRUE(result.complete());
	EXPECT_EQ(result.affected_rows(), 2);
	EXPECT_EQ(result.warning_count(), 0);
	EXPECT_EQ(result.last_insert_id(), 0);
	EXPECT_THAT(std::string(result.info()), HasSubstr("Rows matched"));
}

TEST_F(QueryTest, QuerySyncErrc_SelectOk)
{
	auto result = conn.query("SELECT * FROM empty_table", errc, info);
	validate_no_error();
	EXPECT_TRUE(result.valid());
	EXPECT_FALSE(result.complete());
	validate_2fields_meta(result, "empty_table");
}

TEST_F(QueryTest, QuerySyncErrc_SelectQueryFailed)
{
	auto result = conn.query("SELECT field_varchar, field_bad FROM one_row_table", errc, info);
	validate_sync_fail(mysql::Error::bad_field_error, {"unknown column", "field_bad"});
	EXPECT_FALSE(result.valid());
}

// Query, sync exc
TEST_F(QueryTest, QuerySyncExc_Ok)
{
	auto result = conn.query(
			"INSERT INTO inserts_table (field_varchar, field_date) VALUES ('v0', '2010-10-11')");
	EXPECT_TRUE(result.fields().empty());
	EXPECT_TRUE(result.valid());
	EXPECT_TRUE(result.complete());
	EXPECT_EQ(result.affected_rows(), 1);
	EXPECT_EQ(result.warning_count(), 0);
	EXPECT_GT(result.last_insert_id(), 0);
	EXPECT_EQ(result.info(), "");
}

TEST_F(QueryTest, QuerySyncExc_Error)
{
	validate_sync_fail([this] {
		conn.query("INSERT INTO bad_table (field_varchar, field_date) VALUES ('v0', '2010-10-11')");
	}, mysql::Error::no_such_table, {"table", "doesn't exist", "bad_table"});
}

// Query, async
TEST_F(QueryTest, QueryAsync_InsertQueryOk)
{
	auto [info, result] = conn.async_query(
		"INSERT INTO inserts_table (field_varchar, field_date) VALUES ('v0', '2010-10-11')",
		net::use_future
	).get();
	EXPECT_EQ(info, error_info());
	EXPECT_TRUE(result.fields().empty());
	EXPECT_TRUE(result.valid());
	EXPECT_TRUE(result.complete());
	EXPECT_EQ(result.affected_rows(), 1);
	EXPECT_EQ(result.warning_count(), 0);
	EXPECT_GT(result.last_insert_id(), 0);
	EXPECT_EQ(result.info(), "");
}

TEST_F(QueryTest, QueryAsync_InsertQueryFailed)
{
	validate_async_fail(
		make_query_initiator("INSERT INTO bad_table (field_varchar, field_date) VALUES ('v0', '2010-10-11')"),
		mysql::Error::no_such_table,
		{"table", "doesn't exist", "bad_table"}
	);
}

TEST_F(QueryTest, QueryAsync_UpdateQueryOk)
{
	auto [info, result] = conn.async_query(
		"UPDATE updates_table SET field_int = field_int+1",
		net::use_future
	).get();
	EXPECT_EQ(info, error_info());
	EXPECT_TRUE(result.fields().empty());
	EXPECT_TRUE(result.valid());
	EXPECT_TRUE(result.complete());
	EXPECT_EQ(result.affected_rows(), 2);
	EXPECT_EQ(result.warning_count(), 0);
	EXPECT_EQ(result.last_insert_id(), 0);
	EXPECT_THAT(std::string(result.info()), HasSubstr("Rows matched"));
}

TEST_F(QueryTest, QueryAsync_SelectOk)
{
	auto [info, result] = conn.async_query("SELECT * FROM empty_table", net::use_future).get();
	EXPECT_EQ(info, error_info());
	EXPECT_TRUE(result.valid());
	EXPECT_FALSE(result.complete());
	validate_2fields_meta(result, "empty_table");
}

TEST_F(QueryTest, QueryAsync_SelectQueryFailed)
{
	validate_async_fail(
		make_query_initiator("SELECT field_varchar, field_bad FROM one_row_table"),
		mysql::Error::bad_field_error,
		{"unknown column", "field_bad"}
	);
}

// Some system-level query tests
TEST_F(QueryTest, QueryAndFetch_AliasedTableAndField_MetadataCorrect)
{
	auto result = conn.query("SELECT field_varchar AS field_alias FROM empty_table table_alias");
	meta_validator validator ("table_alias", "empty_table", "field_alias",
			"field_varchar", field_type::varchar);
	validate_meta(result.fields(), {validator});
}

} // anon namespace
