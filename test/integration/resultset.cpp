/*
 * resultset.cpp
 *
 *  Created on: Feb 5, 2020
 *      Author: ruben
 */

#include "integration_test_common.hpp"
#include <boost/asio/use_future.hpp>

using namespace boost::mysql::test;
using boost::mysql::detail::make_error_code;
using boost::mysql::field_metadata;
using boost::mysql::field_type;
using boost::mysql::error_code;
using boost::mysql::error_info;
using boost::mysql::ssl_mode;
using boost::mysql::connection;
using boost::mysql::resultset;
using boost::mysql::prepared_statement;
namespace net = boost::asio;

namespace
{

// Interface to generate a resultset
template <typename Stream>
class resultset_generator
{
public:
	virtual ~resultset_generator() {}
	virtual const char* name() const = 0;
	virtual resultset<Stream> generate(connection<Stream>&, std::string_view) = 0;
};

template <typename Stream>
class text_resultset_generator : public resultset_generator<Stream>
{
public:
	const char* name() const override { return "text"; }
	resultset<Stream> generate(connection<Stream>& conn, std::string_view query) override
	{
		return conn.query(query);
	}
};

template <typename Stream>
class binary_resultset_generator : public resultset_generator<Stream>
{
public:
	const char* name() const override { return "binary"; }
	resultset<Stream> generate(connection<Stream>& conn, std::string_view query) override
	{
		return conn.prepare_statement(query).execute(boost::mysql::no_statement_params);
	}
};

template <typename Stream> text_resultset_generator<Stream> text_obj;
template <typename Stream> binary_resultset_generator<Stream> binary_obj;

// Test parameter
template <typename Stream>
struct resultset_testcase : network_testcase_with_ssl<Stream>
{
	resultset_generator<Stream>* gen;

	resultset_testcase(network_testcase_with_ssl<Stream> base, resultset_generator<Stream>* gen):
		network_testcase_with_ssl<Stream>(base), gen(gen) {}

	std::string name() const
	{
		return network_testcase_with_ssl<Stream>::name() + '_' + gen->name();
	}

	static std::vector<resultset_testcase<Stream>> make_all()
	{
		resultset_generator<Stream>* all_resultset_generators [] = {
			&text_obj<Stream>,
			&binary_obj<Stream>
		};

		std::vector<resultset_testcase<Stream>> res;
		for (auto* gen: all_resultset_generators)
		{
			for (auto base: network_testcase_with_ssl<Stream>::make_all())
			{
				res.push_back(resultset_testcase<Stream>(base, gen));
			}
		}
		return res;
	}
};

template <typename Stream>
struct ResultsetTest : public NetworkTest<Stream, resultset_testcase<Stream>, true>
{
	auto do_generate(std::string_view query) { return this->GetParam().gen->generate(this->conn, query); }
	auto do_fetch_one(resultset<Stream>& r) { return this->GetParam().net->fetch_one(r); }
	auto do_fetch_many(resultset<Stream>& r, std::size_t count) { return this->GetParam().net->fetch_many(r, count); }
	auto do_fetch_all(resultset<Stream>& r) { return this->GetParam().net->fetch_all(r); }

	// FetchOne
	void FetchOne_NoResults()
	{
		auto result = do_generate("SELECT * FROM empty_table");
		EXPECT_TRUE(result.valid());
		EXPECT_FALSE(result.complete());
		EXPECT_EQ(result.fields().size(), 2);

		// Already in the end of the resultset, we receive the EOF
		auto row_result = do_fetch_one(result);
		row_result.validate_no_error();
		EXPECT_EQ(row_result.value, nullptr);
		this->validate_eof(result);

		// Fetching again just returns null
		row_result = do_fetch_one(result);
		row_result.validate_no_error();
		EXPECT_EQ(row_result.value, nullptr);
		this->validate_eof(result);
	}

	void FetchOne_OneRow()
	{
		auto result = do_generate("SELECT * FROM one_row_table");
		EXPECT_TRUE(result.valid());
		EXPECT_FALSE(result.complete());
		EXPECT_EQ(result.fields().size(), 2);

		// Fetch only row
		auto row_result = do_fetch_one(result);
		row_result.validate_no_error();
		ASSERT_NE(row_result.value, nullptr);
		this->validate_2fields_meta(result, "one_row_table");
		EXPECT_EQ(row_result.value->values(), makevalues(1, "f0"));
		EXPECT_FALSE(result.complete());

		// Fetch next: end of resultset
		row_result = do_fetch_one(result);
		row_result.validate_no_error();
		ASSERT_EQ(row_result.value, nullptr);
		this->validate_eof(result);
	}

	void FetchOne_TwoRows()
	{
		auto result = do_generate("SELECT * FROM two_rows_table");
		EXPECT_TRUE(result.valid());
		EXPECT_FALSE(result.complete());
		EXPECT_EQ(result.fields().size(), 2);

		// Fetch first row
		auto row_result = do_fetch_one(result);
		row_result.validate_no_error();
		ASSERT_NE(row_result.value, nullptr);
		this->validate_2fields_meta(result, "two_rows_table");
		EXPECT_EQ(row_result.value->values(), makevalues(1, "f0"));
		EXPECT_FALSE(result.complete());

		// Fetch next row
		row_result = do_fetch_one(result);
		row_result.validate_no_error();
		ASSERT_NE(row_result.value, nullptr);
		this->validate_2fields_meta(result, "two_rows_table");
		EXPECT_EQ(row_result.value->values(), makevalues(2, "f1"));
		EXPECT_FALSE(result.complete());

		// Fetch next: end of resultset
		row_result = do_fetch_one(result);
		row_result.validate_no_error();
		ASSERT_EQ(row_result.value, nullptr);
		this->validate_eof(result);
	}

	// There seems to be no real case where fetch can fail (other than net fails)

	// FetchMany
	void FetchMany_NoResults()
	{
		auto result = do_generate("SELECT * FROM empty_table");

		// Fetch many, but there are no results
		auto rows_result = do_fetch_many(result, 10);
		rows_result.validate_no_error();
		EXPECT_TRUE(rows_result.value.empty());
		EXPECT_TRUE(result.complete());
		this->validate_eof(result);

		// Fetch again, should return OK and empty
		rows_result = do_fetch_many(result, 10);
		rows_result.validate_no_error();
		EXPECT_TRUE(rows_result.value.empty());
		EXPECT_TRUE(result.complete());
		this->validate_eof(result);
	}

	void FetchMany_MoreRowsThanCount()
	{
		auto result = do_generate("SELECT * FROM three_rows_table");

		// Fetch 2, one remaining
		auto rows_result = do_fetch_many(result, 2);
		rows_result.validate_no_error();
		EXPECT_FALSE(result.complete());
		EXPECT_EQ(rows_result.value, (makerows(2, 1, "f0", 2, "f1")));

		// Fetch another two (completes the resultset)
		rows_result = do_fetch_many(result, 2);
		rows_result.validate_no_error();
		EXPECT_TRUE(result.complete());
		this->validate_eof(result);
		EXPECT_EQ(rows_result.value, (makerows(2, 3, "f2")));
	}

	void FetchMany_LessRowsThanCount()
	{
		auto result = do_generate("SELECT * FROM two_rows_table");

		// Fetch 3, resultset exhausted
		auto rows_result = do_fetch_many(result, 3);
		rows_result.validate_no_error();
		EXPECT_EQ(rows_result.value, (makerows(2, 1, "f0", 2, "f1")));
		this->validate_eof(result);
	}

	void FetchMany_SameRowsAsCount()
	{
		auto result = do_generate("SELECT * FROM two_rows_table");

		// Fetch 2, 0 remaining but resultset not exhausted
		auto rows_result = do_fetch_many(result, 2);
		rows_result.validate_no_error();
		EXPECT_FALSE(result.complete());
		EXPECT_EQ(rows_result.value, (makerows(2, 1, "f0", 2, "f1")));

		// Fetch again, exhausts the resultset
		rows_result = do_fetch_many(result, 2);
		rows_result.validate_no_error();
		EXPECT_EQ(rows_result.value.size(), 0);
		this->validate_eof(result);
	}

	void FetchMany_CountEqualsOne()
	{
		auto result = do_generate("SELECT * FROM one_row_table");

		// Fetch 1, 1 remaining
		auto rows_result = do_fetch_many(result, 1);
		rows_result.validate_no_error();
		EXPECT_FALSE(result.complete());
		EXPECT_EQ(rows_result.value, (makerows(2, 1, "f0")));
	}

	// FetchAll
	void FetchAll_NoResults()
	{
		auto result = do_generate("SELECT * FROM empty_table");

		// Fetch many, but there are no results
		auto rows_result = do_fetch_all(result);
		rows_result.validate_no_error();
		EXPECT_TRUE(rows_result.value.empty());
		EXPECT_TRUE(result.complete());

		// Fetch again, should return OK and empty
		rows_result = do_fetch_all(result);
		rows_result.validate_no_error();
		EXPECT_TRUE(rows_result.value.empty());
		this->validate_eof(result);
	}

	void FetchAll_OneRow()
	{
		auto result = do_generate("SELECT * FROM one_row_table");

		auto rows_result = do_fetch_all(result);
		rows_result.validate_no_error();
		EXPECT_TRUE(result.complete());
		EXPECT_EQ(rows_result.value, (makerows(2, 1, "f0")));
	}

	void FetchAll_SeveralRows()
	{
		auto result = do_generate("SELECT * FROM two_rows_table");

		auto rows_result = do_fetch_all(result);
		rows_result.validate_no_error();
		this->validate_eof(result);
		EXPECT_TRUE(result.complete());
		EXPECT_EQ(rows_result.value, (makerows(2, 1, "f0", 2, "f1")));
	}
};

BOOST_MYSQL_NETWORK_TEST_SUITE(ResultsetTest)

BOOST_MYSQL_NETWORK_TEST(ResultsetTest, FetchOne_NoResults)
BOOST_MYSQL_NETWORK_TEST(ResultsetTest, FetchOne_OneRow)
BOOST_MYSQL_NETWORK_TEST(ResultsetTest, FetchOne_TwoRows)
BOOST_MYSQL_NETWORK_TEST(ResultsetTest, FetchMany_NoResults)
BOOST_MYSQL_NETWORK_TEST(ResultsetTest, FetchMany_MoreRowsThanCount)
BOOST_MYSQL_NETWORK_TEST(ResultsetTest, FetchMany_LessRowsThanCount)
BOOST_MYSQL_NETWORK_TEST(ResultsetTest, FetchMany_SameRowsAsCount)
BOOST_MYSQL_NETWORK_TEST(ResultsetTest, FetchMany_CountEqualsOne)
BOOST_MYSQL_NETWORK_TEST(ResultsetTest, FetchAll_NoResults)
BOOST_MYSQL_NETWORK_TEST(ResultsetTest, FetchAll_OneRow)
BOOST_MYSQL_NETWORK_TEST(ResultsetTest, FetchAll_SeveralRows)

}

