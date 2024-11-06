//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/resultset_view.hpp>
#include <boost/mysql/row_view.hpp>
#include <boost/mysql/with_params.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/test/unit_test.hpp>

#include "test_integration/run_coro.hpp"
#include "test_integration/snippets/snippets_fixture.hpp"

namespace mysql = boost::mysql;
namespace asio = boost::asio;
using namespace mysql::test;

namespace {

// Taken here because it's only used in the discussion
void print_employee(mysql::string_view first_name, mysql::string_view last_name)
{
    std::cout << "Employee's name is: " << first_name << ' ' << last_name << std::endl;
}

#ifdef BOOST_ASIO_HAS_CO_AWAIT
asio::awaitable<void> tutorial_updates_transactions(mysql::any_connection& conn)
{
    const char* new_first_name = "John";
    int employee_id = 1;

    {
        //[tutorial_updates_transactions_update
        // Run an UPDATE. We can use with_params to compose it, as with the SELECTs
        // If new_first_name contains 'John' and employee_id contains 42, will run:
        //    UPDATE employee SET first_name = 'John' WHERE id = 42
        // result contains an empty resultset: it has no rows
        mysql::results result;
        co_await conn.async_execute(
            mysql::with_params(
                "UPDATE employee SET first_name = {} WHERE id = {}",
                new_first_name,
                employee_id
            ),
            result
        );
        //]
        //[tutorial_updates_transactions_select
        // Retrieve the newly created employee.
        // As we will see, this is a potential race condition
        // that can be avoided with transactions.
        co_await conn.async_execute(
            mysql::with_params("SELECT first_name, last_name FROM employee WHERE id = {}", employee_id),
            result
        );

        if (result.rows().empty())
        {
            std::cout << "No employee with ID = " << employee_id << std::endl;
        }
        else
        {
            std::cout << "Updated: " << result.rows().at(0).at(0) << " " << result.rows().at(0).at(1)
                      << std::endl;
        }
        //]
    }
    {
        //[tutorial_updates_transactions_txn
        mysql::results empty_result, select_result;

        // Start a transaction block. Subsequent statements will belong
        // to the transaction block, until a COMMIT or ROLLBACK is encountered,
        // or the connection is closed.
        // START TRANSACTION returns no rows.
        co_await conn.async_execute("START TRANSACTION", empty_result);

        // Run the UPDATE as we did before
        co_await conn.async_execute(
            mysql::with_params(
                "UPDATE employee SET first_name = {} WHERE id = {}",
                new_first_name,
                employee_id
            ),
            empty_result
        );

        // Run the SELECT. If a row is returned here, it is the one
        // that we modified.
        co_await conn.async_execute(
            mysql::with_params("SELECT first_name, last_name FROM employee WHERE id = {}", employee_id),
            select_result
        );

        // Commit the transaction. This makes the updated row visible
        // to other transactions and releases any locked rows.
        co_await conn.async_execute("COMMIT", empty_result);

        // Process the retrieved rows
        if (select_result.rows().empty())
        {
            std::cout << "No employee with ID = " << employee_id << std::endl;
        }
        else
        {
            std::cout << "Updated: " << select_result.rows().at(0).at(0) << " "
                      << select_result.rows().at(0).at(1) << std::endl;
        }
        //]
    }
    {
        //[tutorial_updates_transactions_multi_queries
        // Run the 4 statements in a single round-trip.
        // If an error is encountered, successive statements won't be executed
        // and the transaction won't be committed.
        mysql::results result;
        co_await conn.async_execute(
            mysql::with_params(
                "START TRANSACTION;"
                "UPDATE employee SET first_name = {} WHERE id = {};"
                "SELECT first_name, last_name FROM employee WHERE id = {};"
                "COMMIT",
                new_first_name,
                employee_id,
                employee_id
            ),
            result
        );
        //]

        //[tutorial_updates_transactions_dynamic_results
        // Get the 3rd resultset. resultset_view API is similar to results
        mysql::resultset_view select_result = result.at(2);
        if (select_result.rows().empty())
        {
            std::cout << "No employee with ID = " << employee_id << std::endl;
        }
        else
        {
            std::cout << "Updated: " << select_result.rows().at(0).at(0) << " "
                      << select_result.rows().at(0).at(1) << std::endl;
        }
        //]
    }
    {
        //[tutorial_updates_transactions_manual_indices
        // {0} will be replaced by the first format arg, {1} by the second
        mysql::results result;
        co_await conn.async_execute(
            mysql::with_params(
                "START TRANSACTION;"
                "UPDATE employee SET first_name = {0} WHERE id = {1};"
                "SELECT first_name, last_name FROM employee WHERE id = {1};"
                "COMMIT",
                new_first_name,
                employee_id
            ),
            result
        );
        //]
    }
}
#endif

BOOST_FIXTURE_TEST_CASE(section_tutorials, snippets_fixture)
{
    // Tutorial about the static interface
    {
        mysql::results result;
        conn.execute("SELECT first_name, last_name FROM employee WHERE id = 1", result);

        //[tutorial_static_casts
        mysql::row_view employee = result.rows().at(0);
        print_employee(employee.at(0).as_string(), employee.at(1).as_string());
        //]
    }

#ifdef BOOST_ASIO_HAS_CO_AWAIT
    // Tutorial about UPDATEs and transactions
    run_coro(ctx, [&]() { return tutorial_updates_transactions(conn); });
#endif
}

}  // namespace
