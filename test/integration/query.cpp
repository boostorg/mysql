//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/connection.hpp>

#include "integration_test_common.hpp"
#include "metadata_validator.hpp"
#include "test_common.hpp"

using namespace boost::mysql::test;
using boost::mysql::errc;

BOOST_AUTO_TEST_SUITE(test_query)

BOOST_MYSQL_NETWORK_TEST(insert_ok, network_fixture)
{
    setup_and_connect(sample.net);
    start_transaction();

    // Issue query
    conn->query(
            "INSERT INTO inserts_table (field_varchar, field_date) VALUES ('v0', '2010-10-11')",
            *result
    )
        .get();

    // Verify resultset_base
    BOOST_TEST(result->base().fields().empty());
    BOOST_TEST(result->base().valid());
    BOOST_TEST(result->base().complete());
    BOOST_TEST(result->base().affected_rows() == 1u);
    BOOST_TEST(result->base().warning_count() == 0u);
    BOOST_TEST(result->base().last_insert_id() > 0u);
    BOOST_TEST(result->base().info() == "");

    // Verify insertion took place
    BOOST_TEST(this->get_table_size("inserts_table") == 1u);
}

BOOST_MYSQL_NETWORK_TEST(insert_error, network_fixture)
{
    setup_and_connect(sample.net);
    start_transaction();
    auto netresult = conn->query(
        "INSERT INTO bad_table (field_varchar, field_date) VALUES ('v0', '2010-10-11')",
        *result
    );
    netresult.validate_error(errc::no_such_table, {"table", "doesn't exist", "bad_table"});
}

BOOST_MYSQL_NETWORK_TEST(update_ok, network_fixture)
{
    setup_and_connect(sample.net);
    start_transaction();

    // Issue the query
    conn->query("UPDATE updates_table SET field_int = field_int+10", *result).get();

    // Validate resultset_base
    BOOST_TEST(result->base().fields().empty());
    BOOST_TEST(result->base().valid());
    BOOST_TEST(result->base().complete());
    BOOST_TEST(result->base().affected_rows() == 2u);
    BOOST_TEST(result->base().warning_count() == 0u);
    BOOST_TEST(result->base().last_insert_id() == 0u);
    validate_string_contains(std::string(result->base().info()), {"rows matched"});

    // Validate it took effect
    conn->query("SELECT field_int FROM updates_table WHERE field_varchar = 'f0'", *result).get();
    auto updated_value = result->read_all().get().at(0).at(0).as_int64();
    BOOST_TEST(updated_value == 52);  // initial value was 42
}

BOOST_MYSQL_NETWORK_TEST(select_ok, network_fixture)
{
    setup_and_connect(sample.net);

    conn->query("SELECT * FROM empty_table", *result).get();

    BOOST_TEST(result->base().valid());
    BOOST_TEST(!result->base().complete());
    this->validate_2fields_meta(result->base(), "empty_table");
}

BOOST_MYSQL_NETWORK_TEST(select_error, network_fixture)
{
    setup_and_connect(sample.net);
    auto netresult = conn->query("SELECT field_varchar, field_bad FROM one_row_table", *result);
    netresult.validate_error(errc::bad_field_error, {"unknown column", "field_bad"});
}

BOOST_AUTO_TEST_SUITE_END()  // test_query
