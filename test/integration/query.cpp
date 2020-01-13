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

namespace
{

struct QueryTest : public mysql::test::IntegTest
{
	using resultset_type = mysql::resultset<net::ip::tcp::socket>;

	QueryTest()
	{
		handshake();
	}

	void validate_eof(
		const resultset_type& result,
		int affected_rows=0,
		int warnings=0,
		int last_insert=0,
		std::string_view info=""
	)
	{
		EXPECT_TRUE(result.valid());
		EXPECT_TRUE(result.complete());
		EXPECT_EQ(result.affected_rows(), affected_rows);
		EXPECT_EQ(result.warning_count(), warnings);
		EXPECT_EQ(result.last_insert_id(), last_insert);
		EXPECT_EQ(result.info(), info);
	}

	void validate_2fields_meta(
		const std::vector<field_metadata>& fields,
		const std::string& table
	) const
	{
		validate_meta(fields, {
			meta_validator(table, "id", field_type::int_),
			meta_validator(table, "field_varchar", field_type::varchar)
		});
	}

	void validate_2fields_meta(
		const resultset_type& result,
		const std::string& table
	) const
	{
		validate_2fields_meta(result.fields(), table);
	}
};

// Query, sync errc
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

TEST_F(QueryTest, QuerySyncErrc_SelectOk)
{
	auto result = conn.query("SELECT * FROM empty_table", errc);
	ASSERT_EQ(errc, mysql::error_code());
	EXPECT_TRUE(result.valid());
	EXPECT_FALSE(result.complete());
	validate_2fields_meta(result, "empty_table");
}

TEST_F(QueryTest, QuerySyncErrc_SelectQueryFailed)
{
	auto result = conn.query("SELECT field_varchar, field_bad FROM one_row_table", errc);
	ASSERT_EQ(errc, make_error_code(mysql::Error::bad_field_error));
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
	EXPECT_THROW(
		conn.query("INSERT INTO bad_table (field_varchar, field_date) VALUES ('v0', '2010-10-11')"),
		boost::system::system_error
	);
}

// Query, async
TEST_F(QueryTest, QueryAsync_InsertQueryOk)
{
	auto result = conn.async_query(
		"INSERT INTO inserts_table (field_varchar, field_date) VALUES ('v0', '2010-10-11')",
		net::use_future
	).get();
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
	auto fut = conn.async_query(
		"INSERT INTO bad_table (field_varchar, field_date) VALUES ('v0', '2010-10-11')",
		net::use_future
	);
	validate_future_exception(fut, make_error_code(mysql::Error::no_such_table));
}

TEST_F(QueryTest, QueryAsync_UpdateQueryOk)
{
	auto result = conn.async_query(
		"UPDATE updates_table SET field_int = field_int+1",
		net::use_future
	).get();
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
	auto result = conn.async_query("SELECT * FROM empty_table", net::use_future).get();
	EXPECT_TRUE(result.valid());
	EXPECT_FALSE(result.complete());
	validate_2fields_meta(result, "empty_table");
}

TEST_F(QueryTest, QueryAsync_SelectQueryFailed)
{
	auto fut = conn.async_query("SELECT field_varchar, field_bad FROM one_row_table", net::use_future);
	validate_future_exception(fut, make_error_code(mysql::Error::bad_field_error));
}


// FetchOne, sync errc
TEST_F(QueryTest, FetchOneSyncErrc_NoResults)
{
	auto result = conn.query("SELECT * FROM empty_table");
	EXPECT_TRUE(result.valid());
	EXPECT_FALSE(result.complete());
	EXPECT_EQ(result.fields().size(), 2);

	// Already in the end of the resultset, we receive the EOF
	const mysql::row* row = result.fetch_one(errc);
	EXPECT_EQ(errc, mysql::error_code());
	EXPECT_EQ(row, nullptr);
	validate_eof(result);

	// Fetching again just returns null
	row = result.fetch_one(errc);
	EXPECT_EQ(errc, mysql::error_code());
	EXPECT_EQ(row, nullptr);
	validate_eof(result);
}

TEST_F(QueryTest, FetchOneSyncErrc_OneRow)
{
	auto result = conn.query("SELECT * FROM one_row_table");
	EXPECT_TRUE(result.valid());
	EXPECT_FALSE(result.complete());
	EXPECT_EQ(result.fields().size(), 2);

	// Fetch only row
	const mysql::row* row = result.fetch_one(errc);
	ASSERT_EQ(errc, mysql::error_code());
	ASSERT_NE(row, nullptr);
	validate_2fields_meta(result, "one_row_table");
	EXPECT_EQ(row->values(), makevalues(1, "f0"));
	EXPECT_FALSE(result.complete());

	// Fetch next: end of resultset
	row = result.fetch_one(errc);
	ASSERT_EQ(errc, mysql::error_code());
	ASSERT_EQ(row, nullptr);
	validate_eof(result);
}

TEST_F(QueryTest, FetchOneSyncErrc_TwoRows)
{
	auto result = conn.query("SELECT * FROM two_rows_table");
	EXPECT_TRUE(result.valid());
	EXPECT_FALSE(result.complete());
	EXPECT_EQ(result.fields().size(), 2);

	// Fetch first row
	const mysql::row* row = result.fetch_one(errc);
	ASSERT_EQ(errc, mysql::error_code());
	ASSERT_NE(row, nullptr);
	validate_2fields_meta(result, "two_rows_table");
	EXPECT_EQ(row->values(), makevalues(1, "f0"));
	EXPECT_FALSE(result.complete());

	// Fetch next row
	row = result.fetch_one(errc);
	ASSERT_EQ(errc, mysql::error_code());
	ASSERT_NE(row, nullptr);
	validate_2fields_meta(result, "two_rows_table");
	EXPECT_EQ(row->values(), makevalues(2, "f1"));
	EXPECT_FALSE(result.complete());

	// Fetch next: end of resultset
	row = result.fetch_one(errc);
	ASSERT_EQ(errc, mysql::error_code());
	ASSERT_EQ(row, nullptr);
	validate_eof(result);
}

// There seems to be no real case where fetch can fail (other than net fails)

// FetchOne, sync exc
TEST_F(QueryTest, FetchOneSyncExc_TwoRows)
{
	auto result = conn.query("SELECT * FROM two_rows_table");
	EXPECT_TRUE(result.valid());
	EXPECT_FALSE(result.complete());
	EXPECT_EQ(result.fields().size(), 2);

	// Fetch first row
	const mysql::row* row = result.fetch_one();
	ASSERT_NE(row, nullptr);
	validate_2fields_meta(result, "two_rows_table");
	EXPECT_EQ(row->values(), makevalues(1, "f0"));
	EXPECT_FALSE(result.complete());

	// Fetch next row
	row = result.fetch_one();
	ASSERT_NE(row, nullptr);
	validate_2fields_meta(result, "two_rows_table");
	EXPECT_EQ(row->values(), makevalues(2, "f1"));
	EXPECT_FALSE(result.complete());

	// Fetch next: end of resultset
	row = result.fetch_one();
	ASSERT_EQ(row, nullptr);
	validate_eof(result);
}

// FetchOne, async
TEST_F(QueryTest, FetchOneAsync_NoResults)
{
	auto result = conn.query("SELECT * FROM empty_table");

	// Already in the end of the resultset, we receive the EOF
	const auto* row  = result.async_fetch_one(net::use_future).get();
	EXPECT_EQ(row, nullptr);
	validate_eof(result);

	// Fetching again just returns null
	row = result.async_fetch_one(net::use_future).get();
	EXPECT_EQ(row, nullptr);
	validate_eof(result);
}

TEST_F(QueryTest, FetchOneAsync_OneRow)
{
	auto result = conn.query("SELECT * FROM one_row_table");

	// Fetch only row
	const auto* row = result.async_fetch_one(net::use_future).get();
	ASSERT_NE(row, nullptr);
	EXPECT_EQ(row->values(), makevalues(1, "f0"));
	EXPECT_FALSE(result.complete());

	// Fetch next: end of resultset
	row = result.async_fetch_one(net::use_future).get();
	ASSERT_EQ(row, nullptr);
	validate_eof(result);
}

TEST_F(QueryTest, FetchOneAsync_TwoRows)
{
	auto result = conn.query("SELECT * FROM two_rows_table");

	// Fetch first row
	const auto* row = result.async_fetch_one(net::use_future).get();
	ASSERT_NE(row, nullptr);
	EXPECT_EQ(row->values(), makevalues(1, "f0"));
	EXPECT_FALSE(result.complete());

	// Fetch next row
	row = result.async_fetch_one(net::use_future).get();
	ASSERT_NE(row, nullptr);
	EXPECT_EQ(row->values(), makevalues(2, "f1"));
	EXPECT_FALSE(result.complete());

	// Fetch next: end of resultset
	row = result.async_fetch_one(net::use_future).get();
	ASSERT_EQ(row, nullptr);
	validate_eof(result);
}

// FetchMany, sync errc
TEST_F(QueryTest, FetchManySyncErrc_NoResults)
{
	auto result = conn.query("SELECT * FROM empty_table");

	// Fetch many, but there are no results
	auto rows = result.fetch_many(10, errc);
	ASSERT_EQ(errc, error_code());
	EXPECT_TRUE(rows.empty());
	EXPECT_TRUE(result.complete());
	validate_eof(result);

	// Fetch again, should return OK and empty
	rows = result.fetch_many(10, errc);
	ASSERT_EQ(errc, error_code());
	EXPECT_TRUE(rows.empty());
	EXPECT_TRUE(result.complete());
	validate_eof(result);
}

TEST_F(QueryTest, FetchManySyncErrc_MoreRowsThanCount)
{
	auto result = conn.query("SELECT * FROM three_rows_table");

	// Fetch 2, one remaining
	auto rows = result.fetch_many(2, errc);
	ASSERT_EQ(errc, error_code());
	EXPECT_FALSE(result.complete());
	EXPECT_EQ(rows, (makerows(2, 1, "f0", 2, "f1")));

	// Fetch another two (completes the resultset)
	rows = result.fetch_many(2, errc);
	ASSERT_EQ(errc, error_code());
	EXPECT_TRUE(result.complete());
	validate_eof(result);
	EXPECT_EQ(rows, (makerows(2, 3, "f2")));
}

TEST_F(QueryTest, FetchManySyncErrc_LessRowsThanCount)
{
	auto result = conn.query("SELECT * FROM two_rows_table");

	// Fetch 3, resultset exhausted
	auto rows = result.fetch_many(3, errc);
	ASSERT_EQ(errc, error_code());
	EXPECT_EQ(rows, (makerows(2, 1, "f0", 2, "f1")));
	validate_eof(result);
}

TEST_F(QueryTest, FetchManySyncErrc_SameRowsAsCount)
{
	auto result = conn.query("SELECT * FROM two_rows_table");

	// Fetch 2, 0 remaining but resultset not exhausted
	auto rows = result.fetch_many(2, errc);
	ASSERT_EQ(errc, error_code());
	EXPECT_FALSE(result.complete());
	EXPECT_EQ(rows, (makerows(2, 1, "f0", 2, "f1")));

	// Fetch again, exhausts the resultset
	rows = result.fetch_many(2, errc);
	ASSERT_EQ(errc, error_code());
	EXPECT_EQ(rows.size(), 0);
	validate_eof(result);
}

TEST_F(QueryTest, FetchManySyncErrc_CountEqualsOne)
{
	auto result = conn.query("SELECT * FROM one_row_table");

	// Fetch 1, 1 remaining
	auto rows = result.fetch_many(1, errc);
	ASSERT_EQ(errc, error_code());
	EXPECT_FALSE(result.complete());
	EXPECT_EQ(rows, (makerows(2, 1, "f0")));
}

// FetchMany, sync exc
TEST_F(QueryTest, FetchManySyncExc_MoreRowsThanCount)
{
	auto result = conn.query("SELECT * FROM three_rows_table");

	// Fetch 2, one remaining
	auto rows = result.fetch_many(2);
	EXPECT_FALSE(result.complete());
	EXPECT_EQ(rows, (makerows(2, 1, "f0", 2, "f1")));

	// Fetch another two (completes the resultset)
	rows = result.fetch_many(2);
	validate_eof(result);
	EXPECT_EQ(rows, (makerows(2, 3, "f2")));

	// Fetching another time returns empty
	rows = result.fetch_many(2);
	EXPECT_EQ(rows.size(), 0);
}

// FetchMany, async
TEST_F(QueryTest, FetchManyAsync_NoResults)
{
	auto result = conn.query("SELECT * FROM empty_table");

	// Fetch many, but there are no results
	auto rows = result.async_fetch_many(10, net::use_future).get();
	EXPECT_TRUE(rows.empty());
	EXPECT_TRUE(result.complete());
	validate_eof(result);

	// Fetch again, should return OK and empty
	rows = result.async_fetch_many(10, net::use_future).get();
	EXPECT_TRUE(rows.empty());
	EXPECT_TRUE(result.complete());
	validate_eof(result);
}

TEST_F(QueryTest, FetchManyAsync_MoreRowsThanCount)
{
	auto result = conn.query("SELECT * FROM three_rows_table");

	// Fetch 2, one remaining
	auto rows = result.async_fetch_many(2, net::use_future).get();
	EXPECT_FALSE(result.complete());
	EXPECT_EQ(rows, (makerows(2, 1, "f0", 2, "f1")));

	// Fetch another two (completes the resultset)
	rows = result.async_fetch_many(2, net::use_future).get();
	EXPECT_TRUE(result.complete());
	validate_eof(result);
	EXPECT_EQ(rows, (makerows(2, 3, "f2")));
}

TEST_F(QueryTest, FetchManyAsync_LessRowsThanCount)
{
	auto result = conn.query("SELECT * FROM two_rows_table");

	// Fetch 3, resultset exhausted
	auto rows = result.async_fetch_many(3, net::use_future).get();
	EXPECT_EQ(rows, (makerows(2, 1, "f0", 2, "f1")));
	validate_eof(result);
}

TEST_F(QueryTest, FetchManyAsync_SameRowsAsCount)
{
	auto result = conn.query("SELECT * FROM two_rows_table");

	// Fetch 2, 0 remaining but resultset not exhausted
	auto rows = result.async_fetch_many(2, net::use_future).get();
	EXPECT_FALSE(result.complete());
	EXPECT_EQ(rows, (makerows(2, 1, "f0", 2, "f1")));

	// Fetch again, exhausts the resultset
	rows = result.async_fetch_many(2, net::use_future).get();
	EXPECT_EQ(rows.size(), 0);
	validate_eof(result);
}

TEST_F(QueryTest, FetchManyAsync_CountEqualsOne)
{
	auto result = conn.query("SELECT * FROM one_row_table");

	// Fetch 1, 1 remaining
	auto rows = result.async_fetch_many(1, net::use_future).get();
	EXPECT_FALSE(result.complete());
	EXPECT_EQ(rows, (makerows(2, 1, "f0")));
}

// FetchAll, sync errc
TEST_F(QueryTest, FetchAllSyncErrc_NoResults)
{
	auto result = conn.query("SELECT * FROM empty_table");

	// Fetch many, but there are no results
	auto rows = result.fetch_all(errc);
	ASSERT_EQ(errc, error_code());
	EXPECT_TRUE(rows.empty());
	EXPECT_TRUE(result.complete());

	// Fetch again, should return OK and empty
	rows = result.fetch_all(errc);
	ASSERT_EQ(errc, error_code());
	EXPECT_TRUE(rows.empty());
	validate_eof(result);
}

TEST_F(QueryTest, FetchAllSyncErrc_OneRow)
{
	auto result = conn.query("SELECT * FROM one_row_table");

	auto rows = result.fetch_all(errc);
	ASSERT_EQ(errc, error_code());
	EXPECT_TRUE(result.complete());
	EXPECT_EQ(rows, (makerows(2, 1, "f0")));
}

TEST_F(QueryTest, FetchAllSyncErrc_SeveralRows)
{
	auto result = conn.query("SELECT * FROM two_rows_table");

	auto rows = result.fetch_all(errc);
	ASSERT_EQ(errc, error_code());
	validate_eof(result);
	EXPECT_EQ(rows, (makerows(2, 1, "f0", 2, "f1")));
}

// FetchAll, sync exc
TEST_F(QueryTest, FetchAllSyncExc_SeveralRows)
{
	auto result = conn.query("SELECT * FROM two_rows_table");

	auto rows = result.fetch_all();
	ASSERT_EQ(errc, error_code());
	validate_eof(result);
	EXPECT_EQ(rows, (makerows(2, 1, "f0", 2, "f1")));
}

// FetchAll, async
TEST_F(QueryTest, FetchAllAsync_NoResults)
{
	auto result = conn.query("SELECT * FROM empty_table");

	// Fetch many, but there are no results
	auto rows = result.async_fetch_all(net::use_future).get();
	EXPECT_TRUE(rows.empty());
	EXPECT_TRUE(result.complete());

	// Fetch again, should return OK and empty
	rows = result.async_fetch_all(net::use_future).get();
	EXPECT_TRUE(rows.empty());
	validate_eof(result);
}

TEST_F(QueryTest, FetchAllAsync_OneRow)
{
	auto result = conn.query("SELECT * FROM one_row_table");

	auto rows = result.async_fetch_all(net::use_future).get();
	EXPECT_TRUE(result.complete());
	EXPECT_EQ(rows, (makerows(2, 1, "f0")));
}

TEST_F(QueryTest, FetchAllAsync_SeveralRows)
{
	auto result = conn.query("SELECT * FROM two_rows_table");

	auto rows = result.async_fetch_all(net::use_future).get();
	validate_eof(result);
	EXPECT_EQ(rows, (makerows(2, 1, "f0", 2, "f1")));
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





