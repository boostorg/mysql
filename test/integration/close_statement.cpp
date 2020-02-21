/*
 * close_statement.cpp
 *
 *  Created on: Feb 21, 2020
 *      Author: ruben
 */

#include "integration_test_common.hpp"

using namespace mysql::test;

namespace
{

struct CloseStatementTest : NetworkTest<>
{
};

TEST_P(CloseStatementTest, ExistingOrClosedStatement)
{
	auto* net = GetParam();

	// Prepare a statement
	auto stmt = net->prepare_statement(conn, "SELECT * FROM empty_table");
	stmt.validate_no_error();

	// Verify it works fine
	auto exec_result = net->execute_statement(stmt.value, {});
	exec_result.validate_no_error();
	exec_result.value.fetch_all(); // clean all packets sent for this execution

	// Close the statement
	auto close_result = net->close_statement(stmt.value);
	close_result.validate_no_error();

	// Close it again
	close_result = net->close_statement(stmt.value);
	close_result.validate_no_error();

	// Verify close took effect
	exec_result = net->execute_statement(stmt.value, {});
	exec_result.validate_error(mysql::Error::unknown_stmt_handler, {"unknown prepared statement"});
}

MYSQL_NETWORK_TEST_SUITE(CloseStatementTest);

}

