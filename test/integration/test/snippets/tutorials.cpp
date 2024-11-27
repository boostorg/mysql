//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/pfr.hpp>

#include <boost/asio/awaitable.hpp>
#if defined(BOOST_ASIO_HAS_CO_AWAIT) && BOOST_PFR_CORE_NAME_ENABLED

#include <boost/mysql/any_address.hpp>
#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/connection_pool.hpp>
#include <boost/mysql/pfr.hpp>
#include <boost/mysql/pool_params.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/resultset_view.hpp>
#include <boost/mysql/row_view.hpp>
#include <boost/mysql/static_results.hpp>
#include <boost/mysql/with_params.hpp>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/cancel_after.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/system/error_code.hpp>
#include <boost/test/unit_test.hpp>

#include <tuple>

#include "test_common/ci_server.hpp"
#include "test_integration/run_coro.hpp"
#include "test_integration/snippets/credentials.hpp"
#include "test_integration/snippets/snippets_fixture.hpp"

namespace mysql = boost::mysql;
namespace asio = boost::asio;
using namespace mysql::test;

// Common
inline namespace tutorials {
struct employee
{
    std::string first_name;
    std::string last_name;
};
}  // namespace tutorials

namespace {

//
// Tutorial 4: static interface
//
BOOST_FIXTURE_TEST_CASE(section_tutorial_static_interface, snippets_fixture)
{
    mysql::results result;
    conn.execute("SELECT first_name, last_name FROM employee WHERE id = 1", result);
    auto print_employee = [](mysql::string_view, mysql::string_view) {};

    //[tutorial_static_casts
    mysql::row_view employee = result.rows().at(0);
    print_employee(employee.at(0).as_string(), employee.at(1).as_string());
    //]
}

//
// Tutorial 5: updates and txns
//
asio::awaitable<void> tutorial_updates_transactions(mysql::any_connection& conn)
{
    const char* new_first_name = "John";
    int employee_id = 1;

    {
        //[tutorial_updates_transactions_update
        // Run an UPDATE. We can use with_params to compose it, too
        // If new_first_name contains 'John' and employee_id contains 42, this will run:
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

BOOST_FIXTURE_TEST_CASE(section_tutorial_updates_transactions, snippets_fixture)
{
    run_coro(ctx, [&]() { return tutorial_updates_transactions(conn); });
}

//
// Tutorial 6: connection pool
//
mysql::pool_params create_pool_params()
{
    mysql::pool_params res;
    res.server_address.emplace_host_and_port(get_hostname());
    res.username = mysql_username;
    res.password = mysql_password;
    res.database = "boost_mysql_examples";
    return res;
}

BOOST_FIXTURE_TEST_CASE(section_tutorial_connection_pool, snippets_fixture)
{
    run_coro(ctx, [&]() -> asio::awaitable<void> {
        mysql::connection_pool pool(ctx, create_pool_params());
        pool.async_run(asio::detached);

        //[tutorial_connection_pool_get_connection
        // Get a connection from the pool.
        // This will wait until a healthy connection is ready to be used.
        // pooled_connection grants us exclusive access to the connection until
        // the object is destroyed
        mysql::pooled_connection conn = co_await pool.async_get_connection();
        //]
    });
}

//
// Tutorial 7: error handling
//
void log_error(const char*, boost::system::error_code) {}

//[tutorial_error_handling_db_nodiag
asio::awaitable<std::string> get_employee_details(mysql::connection_pool& pool, std::int64_t employee_id)
{
    // Get a connection from the pool.
    // This will wait until a healthy connection is ready to be used.
    // ec is an error code, conn is a pooled_connection
    auto [ec, conn] = co_await pool.async_get_connection(asio::as_tuple);
    if (ec)
    {
        // A connection couldn't be obtained.
        // This may be because a timeout happened.
        log_error("Error in async_get_connection", ec);
        co_return "ERROR";
    }

    // Use the connection normally to query the database.
    mysql::static_results<mysql::pfr_by_name<employee>> result;
    auto [ec2] = co_await conn->async_execute(
        mysql::with_params("SELECT first_name, last_name FROM employee WHERE id = {}", employee_id),
        result,
        asio::as_tuple
    );
    if (ec2)
    {
        log_error("Error running query", ec);
        co_return "ERROR";
    }

    // Handle the result as we did in the previous tutorial
    //<-
    co_return "";
    //->
}
//]

asio::awaitable<void> tutorial_error_handling()
{
    // Setup
    mysql::connection_pool pool(co_await asio::this_coro::executor, create_pool_params());
    pool.async_run(asio::detached);

    {
        //[tutorial_error_handling_get_connection_exc
        // Get a connection from the pool.
        // If an error is encountered (e.g. the session is cancelled by asio::cancel_after),
        // an exception is thrown.
        mysql::pooled_connection conn = co_await pool.async_get_connection();
        //]
    }

    {
        //[tutorial_error_handling_get_connection_as_tuple
        // Passing asio::as_tuple transforms the operation's handler signature:
        //    Original:    void(error_code, mysql::pooled_connection)
        //    Transformed: void(std::tuple<error_code, mysql::pooled_connection>)
        // The transformed signature no longer has an error_code as first parameter,
        // so no automatic error code to exception transformation happens.
        std::tuple<boost::system::error_code, mysql::pooled_connection>
            res = co_await pool.async_get_connection(asio::as_tuple);
        //]
    }

    co_await get_employee_details(pool, 1);
}

BOOST_FIXTURE_TEST_CASE(section_tutorials_cxx20, snippets_fixture)
{
    run_coro(ctx, &tutorial_error_handling);
}

}  // namespace

#endif
