//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "boost/mysql/connection.hpp"
#include <gmock/gmock.h> // for EXPECT_THAT()
#include <boost/asio/use_future.hpp>
#include "metadata_validator.hpp"
#include "integration_test_common.hpp"
#include "test_common.hpp"

namespace net = boost::asio;
using namespace testing;
using namespace boost::mysql::test;
using boost::mysql::detail::make_error_code;
using boost::mysql::field_metadata;
using boost::mysql::field_type;
using boost::mysql::errc;
using boost::mysql::resultset;

namespace
{

template <typename Stream>
struct QueryTest : public NetworkTest<Stream>
{
    QueryTest()
    {
        this->connect(this->GetParam().ssl);
    }

    network_result<resultset<Stream>>
    do_query(boost::string_view sql)
    {
        return this->GetParam().net->query(this->conn, sql);
    }

    void InsertQueryOk()
    {
        this->start_transaction();

        // Issue query
        const char* sql = "INSERT INTO inserts_table (field_varchar, field_date) VALUES ('v0', '2010-10-11')";
        auto result = do_query(sql);

        // Verify resultset
        result.validate_no_error();
        EXPECT_TRUE(result.value.fields().empty());
        EXPECT_TRUE(result.value.valid());
        EXPECT_TRUE(result.value.complete());
        EXPECT_EQ(result.value.affected_rows(), 1);
        EXPECT_EQ(result.value.warning_count(), 0);
        EXPECT_GT(result.value.last_insert_id(), 0);
        EXPECT_EQ(result.value.info(), "");

        // Verify insertion took place
        EXPECT_EQ(this->get_table_size("inserts_table"), 1);
    }

    void InsertQueryFailed()
    {
        this->start_transaction();
        const char* sql = "INSERT INTO bad_table (field_varchar, field_date) VALUES ('v0', '2010-10-11')";
        auto result = do_query(sql);
        result.validate_error(errc::no_such_table, {"table", "doesn't exist", "bad_table"});
        EXPECT_FALSE(result.value.valid());
    }

    void UpdateQueryOk()
    {
        this->start_transaction();

        // Issue the query
        const char* sql = "UPDATE updates_table SET field_int = field_int+10";
        auto result = do_query(sql);

        // Validate resultset
        result.validate_no_error();
        EXPECT_TRUE(result.value.fields().empty());
        EXPECT_TRUE(result.value.valid());
        EXPECT_TRUE(result.value.complete());
        EXPECT_EQ(result.value.affected_rows(), 2);
        EXPECT_EQ(result.value.warning_count(), 0);
        EXPECT_EQ(result.value.last_insert_id(), 0);
        EXPECT_THAT(std::string(result.value.info()), HasSubstr("Rows matched"));

        // Validate it took effect
        result = do_query("SELECT field_int FROM updates_table WHERE field_varchar = 'f0'");
        result.validate_no_error();
        auto updated_value = result.value.fetch_all().at(0).values().at(0).template get<std::int64_t>();
        EXPECT_EQ(updated_value, 52); // initial value was 42
    }

    void SelectOk()
    {
        auto result = do_query("SELECT * FROM empty_table");
        result.validate_no_error();
        EXPECT_TRUE(result.value.valid());
        EXPECT_FALSE(result.value.complete());
        this->validate_2fields_meta(result.value, "empty_table");
    }

    void SelectQueryFailed()
    {
        auto result = do_query("SELECT field_varchar, field_bad FROM one_row_table");
        result.validate_error(errc::bad_field_error, {"unknown column", "field_bad"});
        EXPECT_FALSE(result.value.valid());
    }

    // Some system-level query tests (TODO: this does not feel right here)
    void QueryAndFetch_AliasedTableAndField_MetadataCorrect()
    {
        auto result = do_query("SELECT field_varchar AS field_alias FROM empty_table table_alias");
        meta_validator validator ("table_alias", "empty_table", "field_alias",
                "field_varchar", field_type::varchar);
        result.validate_no_error();
        validate_meta(result.value.fields(), {validator});
    }
};

BOOST_MYSQL_NETWORK_TEST_SUITE(QueryTest)

BOOST_MYSQL_NETWORK_TEST(QueryTest, InsertQueryOk)
BOOST_MYSQL_NETWORK_TEST(QueryTest, InsertQueryFailed)
BOOST_MYSQL_NETWORK_TEST(QueryTest, UpdateQueryOk)
BOOST_MYSQL_NETWORK_TEST(QueryTest, SelectOk)
BOOST_MYSQL_NETWORK_TEST(QueryTest, SelectQueryFailed)
BOOST_MYSQL_NETWORK_TEST(QueryTest, QueryAndFetch_AliasedTableAndField_MetadataCorrect)


} // anon namespace
