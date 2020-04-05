/*
 * prepared_statement_lifecycle.cpp
 *
 *  Created on: Feb 18, 2020
 *      Author: ruben
 */

#include "integration_test_common.hpp"

using namespace boost::mysql::test;
using boost::mysql::row;
using boost::mysql::value;

namespace
{

struct PreparedStatementLifecycleTest : NetworkTest
{
	std::int64_t get_table_size(const std::string& table)
	{
		return std::get<std::int64_t>(
			conn.query("SELECT COUNT(*) FROM " + table).fetch_all().at(0).values().at(0));
	}

	value get_updates_table_value(const std::string& field_varchar="f0")
	{
		return conn.query("SELECT field_int FROM updates_table WHERE field_varchar = '" + field_varchar + "'")
				.fetch_all().at(0).values().at(0);
	}
};

TEST_P(PreparedStatementLifecycleTest, SelectWithParametersMultipleExecutions)
{
	auto* net = GetParam().net;

	// Prepare a statement
	auto stmt = net->prepare_statement(
		conn,
		"SELECT * FROM two_rows_table WHERE id = ? OR field_varchar = ?"
	);
	stmt.validate_no_error();

	// Execute it. Only one row will be returned (because of the id)
	auto result = net->execute_statement(stmt.value, makevalues(1, "non_existent"));
	result.validate_no_error();
	EXPECT_TRUE(result.value.valid());
	EXPECT_FALSE(result.value.complete());
	validate_2fields_meta(result.value, "two_rows_table");

	auto rows = net->fetch_all(result.value);
	rows.validate_no_error();
	ASSERT_EQ(rows.value.size(), 1);
	EXPECT_EQ(static_cast<const row&>(rows.value[0]), (makerow(1, "f0")));
	EXPECT_TRUE(result.value.complete());

	// Execute it again, but with different values. This time, two rows are returned
	result = net->execute_statement(stmt.value, makevalues(1, "f1"));
	result.validate_no_error();
	EXPECT_TRUE(result.value.valid());
	EXPECT_FALSE(result.value.complete());
	validate_2fields_meta(result.value, "two_rows_table");

	rows = net->fetch_all(result.value);
	rows.validate_no_error();
	ASSERT_EQ(rows.value.size(), 2);
	EXPECT_EQ(static_cast<const row&>(rows.value[0]), (makerow(1, "f0")));
	EXPECT_EQ(static_cast<const row&>(rows.value[1]), (makerow(2, "f1")));
	EXPECT_TRUE(result.value.complete());

	// Close it
	auto close_result = net->close_statement(stmt.value);
	close_result.validate_no_error();
}

TEST_P(PreparedStatementLifecycleTest, InsertWithParametersMultipleExecutions)
{
	auto* net = GetParam().net;

	// Get the number of rows before insertion
	auto rows_before = get_table_size("inserts_table");

	// Prepare a statement
	auto stmt = net->prepare_statement(
		conn,
		"INSERT INTO inserts_table (field_varchar) VALUES (?)"
	);
	stmt.validate_no_error();

	// Insert one value
	auto result = net->execute_statement(stmt.value, makevalues("value0"));
	result.validate_no_error();
	EXPECT_TRUE(result.value.valid());
	EXPECT_TRUE(result.value.complete());
	EXPECT_TRUE(result.value.fields().empty());

	// Insert another one
	result = net->execute_statement(stmt.value, makevalues("value1"));
	result.validate_no_error();
	EXPECT_TRUE(result.value.valid());
	EXPECT_TRUE(result.value.complete());
	EXPECT_TRUE(result.value.fields().empty());

	// Validate we did something
	auto rows_after = get_table_size("inserts_table");
	EXPECT_EQ(rows_after, rows_before + 2);

	// Close it
	auto close_result = net->close_statement(stmt.value);
	close_result.validate_no_error();
}

TEST_P(PreparedStatementLifecycleTest, UpdateWithParametersMultipleExecutions)
{
	auto* net = GetParam().net;

	// Prepare a statement
	auto stmt = net->prepare_statement(
		conn,
		"UPDATE updates_table SET field_int = ? WHERE field_varchar = ?"
	);
	stmt.validate_no_error();

	// Set field_int to something
	auto result = net->execute_statement(stmt.value, makevalues(200, "f0"));
	result.validate_no_error();
	EXPECT_TRUE(result.value.valid());
	EXPECT_TRUE(result.value.complete());
	EXPECT_TRUE(result.value.fields().empty());

	// Verify that took effect
	EXPECT_EQ(get_updates_table_value(), value(std::int32_t(200)));

	// Set field_int to something different
	result = net->execute_statement(stmt.value, makevalues(250, "f0"));
	result.validate_no_error();
	EXPECT_TRUE(result.value.valid());
	EXPECT_TRUE(result.value.complete());
	EXPECT_TRUE(result.value.fields().empty());

	// Verify that took effect
	EXPECT_EQ(get_updates_table_value(), value(std::int32_t(250)));

	// Close the statement
	auto close_result = net->close_statement(stmt.value);
	close_result.validate_no_error();
}

TEST_P(PreparedStatementLifecycleTest, MultipleStatements)
{
	auto* net = GetParam().net;

	// Prepare an update
	auto stmt_update = net->prepare_statement(
		conn,
		"UPDATE updates_table SET field_int = ? WHERE field_varchar = ?"
	);
	stmt_update.validate_no_error();

	// Prepare a select
	auto stmt_select = net->prepare_statement(
		conn,
		"SELECT field_int FROM updates_table WHERE field_varchar = ?"
	);
	stmt_select.validate_no_error();

	// They have different IDs
	EXPECT_NE(stmt_update.value.id(), stmt_select.value.id());

	// Execute update
	auto update_result = net->execute_statement(stmt_update.value, makevalues(210, "f0"));
	update_result.validate_no_error();
	EXPECT_TRUE(update_result.value.complete());

	// Execute select
	auto select_result = net->execute_statement(stmt_select.value, makevalues("f0"));
	select_result.validate_no_error();
	auto rows = net->fetch_all(select_result.value);
	rows.validate_no_error();
	EXPECT_EQ(rows.value.size(), 1);
	EXPECT_EQ(static_cast<row&>(rows.value[0]), makerow(210));

	// Execute update again
	update_result = net->execute_statement(stmt_update.value, makevalues(220, "f0"));
	update_result.validate_no_error();
	EXPECT_TRUE(update_result.value.complete());

	// Update no longer needed, close it
	auto close_result = net->close_statement(stmt_update.value);
	close_result.validate_no_error();

	// Execute select again
	select_result = net->execute_statement(stmt_select.value, makevalues("f0"));
	select_result.validate_no_error();
	rows = net->fetch_all(select_result.value);
	rows.validate_no_error();
	EXPECT_EQ(rows.value.size(), 1);
	EXPECT_EQ(static_cast<row&>(rows.value[0]), makerow(220));

	// Close select
	close_result = net->close_statement(stmt_select.value);
	close_result.validate_no_error();
}

TEST_P(PreparedStatementLifecycleTest, InsertWithNullValues)
{
	auto* net = GetParam().net;

	// Statement to perform the updates
	auto stmt = net->prepare_statement(
		conn,
		"UPDATE updates_table SET field_int = ? WHERE field_varchar = 'fnull'"
	);
	stmt.validate_no_error();

	// Set the value we will be updating to something non-NULL
	auto result = net->execute_statement(stmt.value, makevalues(42));
	result.validate_no_error();

	// Verify it took effect
	ASSERT_EQ(get_updates_table_value("fnull"), value(std::int32_t(42)));

	// Update the value to NULL
	result = net->execute_statement(stmt.value, makevalues(nullptr));
	result.validate_no_error();

	// Verify it took effect
	ASSERT_EQ(get_updates_table_value("fnull"), value(nullptr));

	// Close statement
	auto close_result = net->close_statement(stmt.value);
	close_result.validate_no_error();
}

MYSQL_NETWORK_TEST_SUITE(PreparedStatementLifecycleTest);

}


