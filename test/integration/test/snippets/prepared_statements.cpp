//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/awaitable.hpp>
#ifdef BOOST_ASIO_HAS_CO_AWAIT

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/field.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/statement.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/test/unit_test.hpp>

#include <optional>
#include <string_view>
#include <vector>

#include "test_integration/run_coro.hpp"
#include "test_integration/snippets/snippets_fixture.hpp"

namespace asio = boost::asio;
namespace mysql = boost::mysql;
using namespace mysql::test;

namespace {

//[prepared_statements_execute_null
// Inserts a new employee into the database.
// We may not have the salary information for some people,
// so we represent the value as an optional
asio::awaitable<void> insert_employee(
    mysql::any_connection& conn,
    const mysql::statement& stmt,
    std::string_view first_name,
    std::string_view last_name,
    std::optional<int> salary,
    std::string_view company_id
)
{
    // If salary has a value, an integer will be sent to the server.
    // Otherwise, a NULL value will be sent
    mysql::results result;
    co_await conn.async_execute(stmt.bind(first_name, last_name, salary, company_id), result);
}
//]

//[prepared_statements_iterator_range
// Executes the passed statement with the given parameters.
asio::awaitable<void> execute_statement(
    mysql::any_connection& conn,
    const mysql::statement& stmt,
    const std::vector<mysql::field>& params
)
{
    mysql::results result;
    co_await conn.async_execute(stmt.bind(params.begin(), params.end()), result);

    // Do something useful with result
}
//]

asio::awaitable<void> section_main(mysql::any_connection& conn)
{
    {
        //[prepared_statements_prepare
        // Ask the server to prepare a statement to insert a new employee.
        // statement is a lightweight handle to the server-side statement.
        // Each ? is a parameter
        mysql::statement stmt = co_await conn.async_prepare_statement(
            "INSERT INTO employee (first_name, last_name, salary, company_id) VALUES (?, ?, ?, ?)"
        );
        //]

        //[prepared_statements_execute
        // Bind and execute the statement. You must pass one parameter per '?' placeholder in the statement.
        // In the real world, parameters should be runtime values, rather than constants.
        // Note that bind() does not involve communication with the server
        mysql::results result;
        co_await conn.async_execute(stmt.bind("John", "Doe", 40000, "HGS"), result);
        //]

        //[prepared_statements_close
        // Deallocate the statement from the server.
        // Note that closing the connection will also deallocate the statement.
        co_await conn.async_close_statement(stmt);
        //]
    }

    {
        //[prepared_statements_casting
        // Prepare the statement
        mysql::statement stmt = co_await conn.async_prepare_statement(
            "INSERT INTO employee (first_name, last_name, salary, company_id) VALUES (?, ?, ?, ?)"
        );

        // Execute it, passing an 8 byte unsigned integer as the salary value.
        // The salary column was created as an INT (4 byte, signed integer).
        // MySQL will cast the value server-side, and emit an error only if the supplied
        // value is out of range of the target type
        std::uint64_t salary = 45000;
        mysql::results result;
        co_await conn.async_execute(stmt.bind("John", "Doe", salary, "HGS"), result);
        //]

        // Verify that everything's OK with the insertion function
        co_await insert_employee(conn, stmt, "John", "Doe", {}, "HGS");

        // Verify that everything's OK with the range execution function
        // Note: don't inline params in the execute_statement call, as it triggers a gcc ICE
        const std::vector<mysql::field> params{
            mysql::field_view("John"),
            mysql::field_view("Doe"),
            mysql::field_view(35000),
            mysql::field_view("HGS")
        };
        co_await execute_statement(conn, stmt, params);
    }
}

BOOST_FIXTURE_TEST_CASE(section_prepared_statements, snippets_fixture)
{
    run_coro(ctx, [this]() { return section_main(conn); });
}

}  // namespace

#endif
