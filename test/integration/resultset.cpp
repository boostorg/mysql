/*
 * resultset.cpp
 *
 *  Created on: Feb 5, 2020
 *      Author: ruben
 */

#include "integration_test_common.hpp"
#include <boost/asio/use_future.hpp>

using namespace mysql::test;
using mysql::detail::make_error_code;
using mysql::test::meta_validator;
using mysql::test::validate_meta;
using mysql::field_metadata;
using mysql::field_type;
using mysql::error_code;
using mysql::error_info;
namespace net = boost::asio;

namespace
{

struct ResultsetTestParam : named_param
{
	std::string name;
	std::function<mysql::tcp_resultset(mysql::tcp_connection&, std::string_view)> generate_resultset;

	template <typename Callable>
	ResultsetTestParam(std::string name, Callable&& cb):
		name(std::move(name)), generate_resultset(std::forward<Callable>(cb)) {}
};

struct ResultsetTest : public IntegTestAfterHandshake, public testing::WithParamInterface<ResultsetTestParam>
{
};

// FetchOne, sync errc
TEST_P(ResultsetTest, FetchOneSyncErrc_NoResults)
{
	auto result = GetParam().generate_resultset(conn, "SELECT * FROM empty_table");
	EXPECT_TRUE(result.valid());
	EXPECT_FALSE(result.complete());
	EXPECT_EQ(result.fields().size(), 2);

	// Already in the end of the resultset, we receive the EOF
	const mysql::row* row = result.fetch_one(errc, info);
	validate_no_error();
	EXPECT_EQ(row, nullptr);
	validate_eof(result);

	// Fetching again just returns null
	reset_errors();
	row = result.fetch_one(errc, info);
	validate_no_error();
	EXPECT_EQ(row, nullptr);
	validate_eof(result);
}

TEST_P(ResultsetTest, FetchOneSyncErrc_OneRow)
{
	auto result = GetParam().generate_resultset(conn, "SELECT * FROM one_row_table");
	EXPECT_TRUE(result.valid());
	EXPECT_FALSE(result.complete());
	EXPECT_EQ(result.fields().size(), 2);

	// Fetch only row
	const mysql::row* row = result.fetch_one(errc, info);
	validate_no_error();
	ASSERT_NE(row, nullptr);
	validate_2fields_meta(result, "one_row_table");
	EXPECT_EQ(row->values(), makevalues(1, "f0"));
	EXPECT_FALSE(result.complete());

	// Fetch next: end of resultset
	reset_errors();
	row = result.fetch_one(errc, info);
	validate_no_error();
	ASSERT_EQ(row, nullptr);
	validate_eof(result);
}

TEST_P(ResultsetTest, FetchOneSyncErrc_TwoRows)
{
	auto result = GetParam().generate_resultset(conn, "SELECT * FROM two_rows_table");
	EXPECT_TRUE(result.valid());
	EXPECT_FALSE(result.complete());
	EXPECT_EQ(result.fields().size(), 2);

	// Fetch first row
	const mysql::row* row = result.fetch_one(errc, info);
	validate_no_error();
	ASSERT_NE(row, nullptr);
	validate_2fields_meta(result, "two_rows_table");
	EXPECT_EQ(row->values(), makevalues(1, "f0"));
	EXPECT_FALSE(result.complete());

	// Fetch next row
	reset_errors();
	row = result.fetch_one(errc, info);
	validate_no_error();
	ASSERT_NE(row, nullptr);
	validate_2fields_meta(result, "two_rows_table");
	EXPECT_EQ(row->values(), makevalues(2, "f1"));
	EXPECT_FALSE(result.complete());

	// Fetch next: end of resultset
	reset_errors();
	row = result.fetch_one(errc, info);
	validate_no_error();
	ASSERT_EQ(row, nullptr);
	validate_eof(result);
}

// There seems to be no real case where fetch can fail (other than net fails)

// FetchOne, sync exc
TEST_P(ResultsetTest, FetchOneSyncExc_TwoRows)
{
	auto result = GetParam().generate_resultset(conn, "SELECT * FROM two_rows_table");
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
TEST_P(ResultsetTest, FetchOneAsync_NoResults)
{
	auto result = GetParam().generate_resultset(conn, "SELECT * FROM empty_table");

	// Already in the end of the resultset, we receive the EOF
	auto [info, row]  = result.async_fetch_one(net::use_future).get();
	EXPECT_EQ(info, error_info());
	EXPECT_EQ(row, nullptr);
	validate_eof(result);

	// Fetching again just returns null
	std::tie(info, row) = result.async_fetch_one(net::use_future).get();
	EXPECT_EQ(info, error_info());
	EXPECT_EQ(row, nullptr);
	validate_eof(result);
}

TEST_P(ResultsetTest, FetchOneAsync_OneRow)
{
	auto result = GetParam().generate_resultset(conn, "SELECT * FROM one_row_table");

	// Fetch only row
	auto [info, row] = result.async_fetch_one(net::use_future).get();
	EXPECT_EQ(info, error_info());
	ASSERT_NE(row, nullptr);
	EXPECT_EQ(row->values(), makevalues(1, "f0"));
	EXPECT_FALSE(result.complete());

	// Fetch next: end of resultset
	reset_errors();
	std::tie(info, row) = result.async_fetch_one(net::use_future).get();
	EXPECT_EQ(info, error_info());
	ASSERT_EQ(row, nullptr);
	validate_eof(result);
}

TEST_P(ResultsetTest, FetchOneAsync_TwoRows)
{
	auto result = GetParam().generate_resultset(conn, "SELECT * FROM two_rows_table");

	// Fetch first row
	auto [info, row] = result.async_fetch_one(net::use_future).get();
	EXPECT_EQ(info, error_info());
	ASSERT_NE(row, nullptr);
	EXPECT_EQ(row->values(), makevalues(1, "f0"));
	EXPECT_FALSE(result.complete());

	// Fetch next row
	reset_errors();
	std::tie(info, row) = result.async_fetch_one(net::use_future).get();
	EXPECT_EQ(info, error_info());
	ASSERT_NE(row, nullptr);
	EXPECT_EQ(row->values(), makevalues(2, "f1"));
	EXPECT_FALSE(result.complete());

	// Fetch next: end of resultset
	reset_errors();
	std::tie(info, row) = result.async_fetch_one(net::use_future).get();
	EXPECT_EQ(info, error_info());
	ASSERT_EQ(row, nullptr);
	validate_eof(result);
}

// FetchMany, sync errc
TEST_P(ResultsetTest, FetchManySyncErrc_NoResults)
{
	auto result = GetParam().generate_resultset(conn, "SELECT * FROM empty_table");

	// Fetch many, but there are no results
	auto rows = result.fetch_many(10, errc, info);
	validate_no_error();
	EXPECT_TRUE(rows.empty());
	EXPECT_TRUE(result.complete());
	validate_eof(result);

	// Fetch again, should return OK and empty
	reset_errors();
	rows = result.fetch_many(10, errc, info);
	ASSERT_EQ(errc, error_code());
	EXPECT_EQ(info, error_info());
	EXPECT_TRUE(rows.empty());
	EXPECT_TRUE(result.complete());
	validate_eof(result);
}

TEST_P(ResultsetTest, FetchManySyncErrc_MoreRowsThanCount)
{
	auto result = GetParam().generate_resultset(conn, "SELECT * FROM three_rows_table");

	// Fetch 2, one remaining
	auto rows = result.fetch_many(2, errc, info);
	validate_no_error();
	EXPECT_FALSE(result.complete());
	EXPECT_EQ(rows, (makerows(2, 1, "f0", 2, "f1")));

	// Fetch another two (completes the resultset)
	reset_errors();
	rows = result.fetch_many(2, errc, info);
	validate_no_error();
	EXPECT_TRUE(result.complete());
	validate_eof(result);
	EXPECT_EQ(rows, (makerows(2, 3, "f2")));
}

TEST_P(ResultsetTest, FetchManySyncErrc_LessRowsThanCount)
{
	auto result = GetParam().generate_resultset(conn, "SELECT * FROM two_rows_table");

	// Fetch 3, resultset exhausted
	auto rows = result.fetch_many(3, errc, info);
	validate_no_error();
	EXPECT_EQ(rows, (makerows(2, 1, "f0", 2, "f1")));
	validate_eof(result);
}

TEST_P(ResultsetTest, FetchManySyncErrc_SameRowsAsCount)
{
	auto result = GetParam().generate_resultset(conn, "SELECT * FROM two_rows_table");

	// Fetch 2, 0 remaining but resultset not exhausted
	auto rows = result.fetch_many(2, errc, info);
	validate_no_error();
	EXPECT_FALSE(result.complete());
	EXPECT_EQ(rows, (makerows(2, 1, "f0", 2, "f1")));

	// Fetch again, exhausts the resultset
	reset_errors();
	rows = result.fetch_many(2, errc, info);
	validate_no_error();
	EXPECT_EQ(rows.size(), 0);
	validate_eof(result);
}

TEST_P(ResultsetTest, FetchManySyncErrc_CountEqualsOne)
{
	auto result = GetParam().generate_resultset(conn, "SELECT * FROM one_row_table");

	// Fetch 1, 1 remaining
	auto rows = result.fetch_many(1, errc, info);
	validate_no_error();
	EXPECT_FALSE(result.complete());
	EXPECT_EQ(rows, (makerows(2, 1, "f0")));
}

// FetchMany, sync exc
TEST_P(ResultsetTest, FetchManySyncExc_MoreRowsThanCount)
{
	auto result = GetParam().generate_resultset(conn, "SELECT * FROM three_rows_table");

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
TEST_P(ResultsetTest, FetchManyAsync_NoResults)
{
	auto result = GetParam().generate_resultset(conn, "SELECT * FROM empty_table");

	// Fetch many, but there are no results
	auto [info, rows] = result.async_fetch_many(10, net::use_future).get();
	EXPECT_EQ(info, error_info());
	EXPECT_TRUE(rows.empty());
	EXPECT_TRUE(result.complete());
	validate_eof(result);

	// Fetch again, should return OK and empty
	reset_errors();
	std::tie(info, rows) = result.async_fetch_many(10, net::use_future).get();
	EXPECT_EQ(info, error_info());
	EXPECT_TRUE(rows.empty());
	EXPECT_TRUE(result.complete());
	validate_eof(result);
}

TEST_P(ResultsetTest, FetchManyAsync_MoreRowsThanCount)
{
	auto result = GetParam().generate_resultset(conn, "SELECT * FROM three_rows_table");

	// Fetch 2, one remaining
	auto [info, rows] = result.async_fetch_many(2, net::use_future).get();
	EXPECT_EQ(info, error_info());
	EXPECT_FALSE(result.complete());
	EXPECT_EQ(rows, (makerows(2, 1, "f0", 2, "f1")));

	// Fetch another two (completes the resultset)
	std::tie(info, rows) = result.async_fetch_many(2, net::use_future).get();
	EXPECT_EQ(info, error_info());
	EXPECT_TRUE(result.complete());
	validate_eof(result);
	EXPECT_EQ(rows, (makerows(2, 3, "f2")));
}

TEST_P(ResultsetTest, FetchManyAsync_LessRowsThanCount)
{
	auto result = GetParam().generate_resultset(conn, "SELECT * FROM two_rows_table");

	// Fetch 3, resultset exhausted
	auto [info, rows] = result.async_fetch_many(3, net::use_future).get();
	EXPECT_EQ(info, error_info());
	EXPECT_EQ(rows, (makerows(2, 1, "f0", 2, "f1")));
	validate_eof(result);
}

TEST_P(ResultsetTest, FetchManyAsync_SameRowsAsCount)
{
	auto result = GetParam().generate_resultset(conn, "SELECT * FROM two_rows_table");

	// Fetch 2, 0 remaining but resultset not exhausted
	auto [info, rows] = result.async_fetch_many(2, net::use_future).get();
	EXPECT_EQ(info, error_info());
	EXPECT_FALSE(result.complete());
	EXPECT_EQ(rows, (makerows(2, 1, "f0", 2, "f1")));

	// Fetch again, exhausts the resultset
	reset_errors();
	std::tie(info, rows) = result.async_fetch_many(2, net::use_future).get();
	EXPECT_EQ(info, error_info());
	EXPECT_EQ(rows.size(), 0);
	validate_eof(result);
}

TEST_P(ResultsetTest, FetchManyAsync_CountEqualsOne)
{
	auto result = GetParam().generate_resultset(conn, "SELECT * FROM one_row_table");

	// Fetch 1, 1 remaining
	auto [info, rows] = result.async_fetch_many(1, net::use_future).get();
	EXPECT_EQ(info, error_info());
	EXPECT_FALSE(result.complete());
	EXPECT_EQ(rows, (makerows(2, 1, "f0")));
}

// FetchAll, sync errc
TEST_P(ResultsetTest, FetchAllSyncErrc_NoResults)
{
	auto result = GetParam().generate_resultset(conn, "SELECT * FROM empty_table");

	// Fetch many, but there are no results
	auto rows = result.fetch_all(errc, info);
	validate_no_error();
	EXPECT_TRUE(rows.empty());
	EXPECT_TRUE(result.complete());

	// Fetch again, should return OK and empty
	reset_errors();
	rows = result.fetch_all(errc, info);
	validate_no_error();
	EXPECT_TRUE(rows.empty());
	validate_eof(result);
}

TEST_P(ResultsetTest, FetchAllSyncErrc_OneRow)
{
	auto result = GetParam().generate_resultset(conn, "SELECT * FROM one_row_table");

	auto rows = result.fetch_all(errc, info);
	validate_no_error();
	EXPECT_TRUE(result.complete());
	EXPECT_EQ(rows, (makerows(2, 1, "f0")));
}

TEST_P(ResultsetTest, FetchAllSyncErrc_SeveralRows)
{
	auto result = GetParam().generate_resultset(conn, "SELECT * FROM two_rows_table");

	auto rows = result.fetch_all(errc, info);
	validate_no_error();
	validate_eof(result);
	EXPECT_EQ(rows, (makerows(2, 1, "f0", 2, "f1")));
}

// FetchAll, sync exc
TEST_P(ResultsetTest, FetchAllSyncExc_SeveralRows)
{
	auto result = GetParam().generate_resultset(conn, "SELECT * FROM two_rows_table");
	auto rows = result.fetch_all();
	validate_eof(result);
	EXPECT_EQ(rows, (makerows(2, 1, "f0", 2, "f1")));
}

// FetchAll, async
TEST_P(ResultsetTest, FetchAllAsync_NoResults)
{
	auto result = GetParam().generate_resultset(conn, "SELECT * FROM empty_table");

	// Fetch many, but there are no results
	auto [info, rows] = result.async_fetch_all(net::use_future).get();
	EXPECT_EQ(info, error_info());
	EXPECT_TRUE(rows.empty());
	EXPECT_TRUE(result.complete());

	// Fetch again, should return OK and empty
	reset_errors();
	std::tie(info, rows) = result.async_fetch_all(net::use_future).get();
	EXPECT_EQ(info, error_info());
	EXPECT_TRUE(rows.empty());
	validate_eof(result);
}

TEST_P(ResultsetTest, FetchAllAsync_OneRow)
{
	auto result = GetParam().generate_resultset(conn, "SELECT * FROM one_row_table");

	auto [info, rows] = result.async_fetch_all(net::use_future).get();
	EXPECT_EQ(info, error_info());
	EXPECT_TRUE(result.complete());
	EXPECT_EQ(rows, (makerows(2, 1, "f0")));
}

TEST_P(ResultsetTest, FetchAllAsync_SeveralRows)
{
	auto result = GetParam().generate_resultset(conn, "SELECT * FROM two_rows_table");

	auto [info, rows] = result.async_fetch_all(net::use_future).get();
	EXPECT_EQ(info, error_info());
	validate_eof(result);
	EXPECT_EQ(rows, (makerows(2, 1, "f0", 2, "f1")));
}

mysql::tcp_resultset make_text_resultset(mysql::tcp_connection& conn, std::string_view query)
{
	return conn.query(query);
}

mysql::tcp_resultset make_binary_resultset(mysql::tcp_connection& conn, std::string_view query)
{
	return conn.prepare_statement(query).execute(mysql::no_statement_params);
}


INSTANTIATE_TEST_SUITE_P(Default, ResultsetTest, testing::Values(
	ResultsetTestParam("text", &make_text_resultset),
	ResultsetTestParam("binary", &make_binary_resultset)
), test_name_generator);

}

