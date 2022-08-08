//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "er_connection.hpp"
#include "integration_test_common.hpp"

using namespace boost::mysql::test;
using boost::mysql::field_view;

BOOST_AUTO_TEST_SUITE(test_prepared_statement_lifecycle)

field_view get_updates_table_value(
    er_connection& conn,
    const std::string& field_varchar="f0"
)
{
    return conn.query(
        "SELECT field_int FROM updates_table WHERE field_varchar = '" + field_varchar + "'").get()
            ->read_all().get().at(0).values().at(0);
}

BOOST_MYSQL_NETWORK_TEST(select_with_parameters_multiple_executions, network_fixture)
{
    setup_and_connect(sample.net);

    // Prepare a statement
    auto stmt = conn->prepare_statement("SELECT * FROM two_rows_table WHERE id = ? OR field_varchar = ?").get();

    // Execute it. Only one row will be returned (because of the id)
    auto result = stmt->execute_container(make_value_vector(1, "non_existent")).get();
    BOOST_TEST(result->valid());
    BOOST_TEST(!result->complete());
    validate_2fields_meta(*result, "two_rows_table");

    auto rows = result->read_all().get();
    BOOST_TEST_REQUIRE(rows.size() == 1u);
    BOOST_TEST((rows[0] == makerow(1, "f0")));
    BOOST_TEST(result->complete());

    // Execute it again, but with different values. This time, two rows are returned
    result = stmt->execute_container(make_value_vector(1, "f1")).get();
    BOOST_TEST(result->valid());
    BOOST_TEST(!result->complete());
    validate_2fields_meta(*result, "two_rows_table");

    rows = result->read_all().get();
    BOOST_TEST_REQUIRE(rows.size() == 2u);
    BOOST_TEST((rows[0] == makerow(1, "f0")));
    BOOST_TEST((rows[1] == makerow(2, "f1")));
    BOOST_TEST(result->complete());

    // Close it
    stmt->close().validate_no_error();
}

BOOST_MYSQL_NETWORK_TEST(insert_with_parameters_multiple_executions, network_fixture)
{
    setup_and_connect(sample.net);
    start_transaction();

    // Prepare a statement
    auto stmt = conn->prepare_statement("INSERT INTO inserts_table (field_varchar) VALUES (?)").get();

    // Insert one value
    auto result = stmt->execute_container(make_value_vector("value0")).get();
    BOOST_TEST(result->valid());
    BOOST_TEST(result->complete());
    BOOST_TEST(result->fields().empty());

    // Insert another one
    result = stmt->execute_container(make_value_vector("value1")).get();
    BOOST_TEST(result->valid());
    BOOST_TEST(result->complete());
    BOOST_TEST(result->fields().empty());

    // Validate that the insertions took place
    BOOST_TEST(get_table_size("inserts_table") == 2);

    // Close it
    stmt->close().validate_no_error();
}

BOOST_MYSQL_NETWORK_TEST(update_with_parameters_multiple_executions, network_fixture)
{
    setup_and_connect(sample.net);
    start_transaction();

    // Prepare a statement
    auto stmt = conn->prepare_statement("UPDATE updates_table SET field_int = ? WHERE field_varchar = ?").get();

    // Set field_int to something
    auto result = stmt->execute_container(make_value_vector(200, "f0")).get();
    BOOST_TEST(result->valid());
    BOOST_TEST(result->complete());
    BOOST_TEST(result->fields().empty());

    // Verify that took effect
    BOOST_TEST(get_updates_table_value(*conn) == field_view(std::int32_t(200)));

    // Set field_int to something different
    result = stmt->execute_container(make_value_vector(250, "f0")).get();
    BOOST_TEST(result->valid());
    BOOST_TEST(result->complete());
    BOOST_TEST(result->fields().empty());

    // Verify that took effect
    BOOST_TEST(get_updates_table_value(*conn) == field_view(std::int32_t(250)));

    // Close the statement
    stmt->close().validate_no_error();
}

BOOST_MYSQL_NETWORK_TEST(multiple_statements, network_fixture)
{
    setup_and_connect(sample.net);
    start_transaction();

    // Prepare an update and a select
    auto stmt_update = conn->prepare_statement("UPDATE updates_table SET field_int = ? WHERE field_varchar = ?").get();
    auto stmt_select = conn->prepare_statement("SELECT field_int FROM updates_table WHERE field_varchar = ?").get();

    // They have different IDs
    BOOST_TEST(stmt_update->id() != stmt_select->id());

    // Execute update
    auto update_result = stmt_update->execute_container(make_value_vector(210, "f0")).get();
    BOOST_TEST(update_result->complete());

    // Execute select
    auto select_result = stmt_select->execute_container(make_value_vector("f0")).get();
    auto rows = select_result->read_all().get();
    BOOST_TEST(rows.size() == 1u);
    BOOST_TEST(rows[0] == makerow(210));

    // Execute update again
    update_result = stmt_update->execute_container(make_value_vector(220, "f0")).get();
    BOOST_TEST(update_result->complete());

    // Update no longer needed, close it
    stmt_update->close().validate_no_error();

    // Execute select again
    select_result = stmt_select->execute_container(make_value_vector("f0")).get();
    rows = select_result->read_all().get();
    BOOST_TEST(rows.size() == 1u);
    BOOST_TEST(rows[0] == makerow(220));

    // Close select
    stmt_select->close().validate_no_error();
}

BOOST_MYSQL_NETWORK_TEST(insert_with_null_values, network_fixture)
{
    setup_and_connect(sample.net);
    start_transaction();

    // Statement to perform the updates
    auto stmt = conn->prepare_statement("UPDATE updates_table SET field_int = ? WHERE field_varchar = 'fnull'").get();

    // Set the value we will be updating to something non-NULL
    auto result = stmt->execute_container(make_value_vector(42)).get();

    // Verify it took effect
    BOOST_TEST_REQUIRE((get_updates_table_value(*conn, "fnull") == field_view(std::int32_t(42))));

    // Update the value to NULL
    result = stmt->execute_container(make_value_vector(nullptr)).get();

    // Verify it took effect
    BOOST_TEST_REQUIRE((get_updates_table_value(*conn, "fnull") == field_view(nullptr)));

    // Close statement
    stmt->close().validate_no_error();
}

BOOST_AUTO_TEST_SUITE_END() // test_prepared_statement_lifecycle
