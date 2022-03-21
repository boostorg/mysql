//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/connection.hpp>
#include "metadata_validator.hpp"
#include "integration_test_common.hpp"
#include "test_common.hpp"

using namespace boost::mysql::test;
using boost::mysql::errc;

BOOST_AUTO_TEST_SUITE(test_query)

BOOST_MYSQL_NETWORK_TEST(insert_ok, network_fixture)
{
    setup_and_connect(sample.net);
    start_transaction();

    // Issue query
    auto result = conn->query(
        "INSERT INTO inserts_table (field_varchar, field_date) VALUES ('v0', '2010-10-11')").get();

    // Verify resultset
    BOOST_TEST(result->fields().empty());
    BOOST_TEST(result->valid());
    BOOST_TEST(result->complete());
    BOOST_TEST(result->affected_rows() == 1u);
    BOOST_TEST(result->warning_count() == 0u);
    BOOST_TEST(result->last_insert_id() > 0u);
    BOOST_TEST(result->info() == "");

    // Verify insertion took place
    BOOST_TEST(this->get_table_size("inserts_table") == 1u);
}

BOOST_MYSQL_NETWORK_TEST(insert_error, network_fixture)
{
    setup_and_connect(sample.net);
    start_transaction();
    auto result = conn->query(
        "INSERT INTO bad_table (field_varchar, field_date) VALUES ('v0', '2010-10-11')");
    result.validate_error(errc::no_such_table, {"table", "doesn't exist", "bad_table"});
}

BOOST_MYSQL_NETWORK_TEST(update_ok, network_fixture)
{
    setup_and_connect(sample.net);
    start_transaction();

    // Issue the query
    auto result = conn->query("UPDATE updates_table SET field_int = field_int+10").get();

    // Validate resultset
    BOOST_TEST(result->fields().empty());
    BOOST_TEST(result->valid());
    BOOST_TEST(result->complete());
    BOOST_TEST(result->affected_rows() == 2u);
    BOOST_TEST(result->warning_count() == 0u);
    BOOST_TEST(result->last_insert_id() == 0u);
    validate_string_contains(std::string(result->info()), {"rows matched"});

    // Validate it took effect
    result = conn->query("SELECT field_int FROM updates_table WHERE field_varchar = 'f0'").get();
    auto updated_value = result->read_all().get().at(0).values().at(0).template get<std::int64_t>();
    BOOST_TEST(updated_value == 52); // initial value was 42
}

BOOST_MYSQL_NETWORK_TEST(select_ok, network_fixture)
{
    setup_and_connect(sample.net);
    auto result = conn->query("SELECT * FROM empty_table").get();
    BOOST_TEST(result->valid());
    BOOST_TEST(!result->complete());
    this->validate_2fields_meta(*result, "empty_table");
}

BOOST_MYSQL_NETWORK_TEST(select_error, network_fixture)
{
    setup_and_connect(sample.net);
    auto result = conn->query("SELECT field_varchar, field_bad FROM one_row_table");
    result.validate_error(errc::bad_field_error, {"unknown column", "field_bad"});
}

BOOST_AUTO_TEST_SUITE_END() // test_query
