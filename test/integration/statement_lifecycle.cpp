//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "er_connection.hpp"
#include "er_resultset.hpp"
#include "integration_test_common.hpp"
#include "test_common.hpp"

using namespace boost::mysql::test;
using boost::mysql::field_view;

namespace {

BOOST_AUTO_TEST_SUITE(test_statement_lifecycle)

struct statement_lifecycle_fixture : network_fixture
{
    field_view get_updates_table_value(const std::string& field_varchar = "f0")
    {
        conn->query(
                "SELECT field_int FROM updates_table WHERE field_varchar = '" + field_varchar + "'",
                *result
        )
            .validate_no_error();
        return result->read_all().get().at(0).at(0);
    }
};

BOOST_MYSQL_NETWORK_TEST(select_with_parameters_multiple_executions, statement_lifecycle_fixture)
{
    setup_and_connect(sample.net);

    // Prepare a statement
    conn->prepare_statement("SELECT * FROM two_rows_table WHERE id = ? OR field_varchar = ?", *stmt)
        .validate_no_error();

    // Execute it. Only one row will be returned (because of the id)
    stmt->execute_collection(make_fv_vector(1, "non_existent"), *result).validate_no_error();
    BOOST_TEST(result->base().valid());
    BOOST_TEST(!result->base().complete());
    validate_2fields_meta(result->base(), "two_rows_table");

    auto rows = result->read_all().get();
    BOOST_TEST_REQUIRE(rows.size() == 1u);
    BOOST_TEST((rows[0] == makerow(1, "f0")));
    BOOST_TEST(result->base().complete());

    // Execute it again, but with different values. This time, two rows are returned
    stmt->execute_collection(make_fv_vector(1, "f1"), *result).validate_no_error();
    BOOST_TEST(result->base().valid());
    BOOST_TEST(!result->base().complete());
    validate_2fields_meta(result->base(), "two_rows_table");

    rows = result->read_all().get();
    BOOST_TEST_REQUIRE(rows.size() == 2u);
    BOOST_TEST((rows[0] == makerow(1, "f0")));
    BOOST_TEST((rows[1] == makerow(2, "f1")));
    BOOST_TEST(result->base().complete());

    // Close it
    stmt->close().validate_no_error();
}

BOOST_MYSQL_NETWORK_TEST(insert_with_parameters_multiple_executions, statement_lifecycle_fixture)
{
    setup_and_connect(sample.net);
    start_transaction();

    // Prepare a statement
    conn->prepare_statement("INSERT INTO inserts_table (field_varchar) VALUES (?)", *stmt)
        .validate_no_error();

    // Insert one value
    stmt->execute_collection(make_fv_vector("value0"), *result).validate_no_error();
    BOOST_TEST(result->base().valid());
    BOOST_TEST(result->base().complete());
    BOOST_TEST(result->base().meta().empty());

    // Insert another one
    stmt->execute_collection(make_fv_vector("value1"), *result).validate_no_error();
    BOOST_TEST(result->base().valid());
    BOOST_TEST(result->base().complete());
    BOOST_TEST(result->base().meta().empty());

    // Validate that the insertions took place
    BOOST_TEST(get_table_size("inserts_table") == 2);

    // Close it
    stmt->close().validate_no_error();
}

BOOST_MYSQL_NETWORK_TEST(update_with_parameters_multiple_executions, statement_lifecycle_fixture)
{
    setup_and_connect(sample.net);
    start_transaction();

    // Prepare a statement
    conn->prepare_statement("UPDATE updates_table SET field_int = ? WHERE field_varchar = ?", *stmt)
        .validate_no_error();

    // Set field_int to something
    stmt->execute_collection(make_fv_vector(200, "f0"), *result).validate_no_error();
    BOOST_TEST(result->base().valid());
    BOOST_TEST(result->base().complete());
    BOOST_TEST(result->base().meta().empty());

    // Verify that took effect
    BOOST_TEST(get_updates_table_value() == field_view(200));

    // Set field_int to something different
    stmt->execute_collection(make_fv_vector(250, "f0"), *result).validate_no_error();
    BOOST_TEST(result->base().valid());
    BOOST_TEST(result->base().complete());
    BOOST_TEST(result->base().meta().empty());

    // Verify that took effect
    BOOST_TEST(get_updates_table_value() == field_view(250));

    // Close the statement
    stmt->close().validate_no_error();
}

BOOST_MYSQL_NETWORK_TEST(multiple_statements, statement_lifecycle_fixture)
{
    setup_and_connect(sample.net);
    start_transaction();
    auto stmt_update = var->create_statement();
    auto stmt_select = var->create_statement();

    // Prepare an update and a select
    conn->prepare_statement(
            "UPDATE updates_table SET field_int = ? WHERE field_varchar = ?",
            *stmt_update
    )
        .validate_no_error();
    conn->prepare_statement(
            "SELECT field_int FROM updates_table WHERE field_varchar = ?",
            *stmt_select
    )
        .validate_no_error();

    // They have different IDs
    BOOST_TEST(stmt_update->base().id() != stmt_select->base().id());

    // Execute update
    stmt_update->execute_collection(make_fv_vector(210, "f0"), *result).validate_no_error();
    BOOST_TEST(result->base().complete());

    // Execute select
    stmt_select->execute_collection(make_fv_vector("f0"), *result).validate_no_error();
    auto rows = result->read_all().get();
    BOOST_TEST(rows.size() == 1u);
    BOOST_TEST(rows[0] == makerow(210));

    // Execute update again
    stmt_update->execute_collection(make_fv_vector(220, "f0"), *result).validate_no_error();
    BOOST_TEST(result->base().complete());

    // Update no longer needed, close it
    stmt_update->close().validate_no_error();
    BOOST_TEST(!stmt_update->base().valid());

    // Execute select again
    stmt_select->execute_collection(make_fv_vector("f0"), *result).validate_no_error();
    rows = result->read_all().get();
    BOOST_TEST(rows.size() == 1u);
    BOOST_TEST(rows[0] == makerow(220));

    // Close select
    stmt_select->close().validate_no_error();
}

BOOST_MYSQL_NETWORK_TEST(insert_with_null_values, statement_lifecycle_fixture)
{
    setup_and_connect(sample.net);
    start_transaction();

    // Statement to perform the updates
    conn->prepare_statement(
            "UPDATE updates_table SET field_int = ? WHERE field_varchar = 'fnull'",
            *stmt
    )
        .validate_no_error();

    // Set the value we will be updating to something non-NULL
    stmt->execute_collection(make_fv_vector(42), *result).validate_no_error();

    // Verify it took effect
    BOOST_TEST_REQUIRE((get_updates_table_value("fnull") == field_view(42)));

    // Update the value to NULL
    stmt->execute_collection(make_fv_vector(nullptr), *result).validate_no_error();

    // Verify it took effect
    BOOST_TEST_REQUIRE((get_updates_table_value("fnull") == field_view(nullptr)));

    // Close statement
    stmt->close().validate_no_error();
}

BOOST_AUTO_TEST_SUITE_END()  // test_prepared_statement_lifecycle

}  // namespace
