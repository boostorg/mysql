//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/awaitable.hpp>
#ifdef BOOST_ASIO_HAS_CO_AWAIT

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/common_server_errc.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/with_params.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/test/unit_test.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "test_common/printing.hpp"
#include "test_integration/run_coro.hpp"
#include "test_integration/snippets/snippets_fixture.hpp"

namespace mysql = boost::mysql;
namespace asio = boost::asio;
using namespace boost::mysql::test;

namespace {

std::string get_name() { return "John"; }

asio::awaitable<void> section_main(mysql::any_connection& conn)
{
    {
        //[text_queries_with_params_simple
        std::string employee_name = get_name();  // employee_name is an untrusted string
        mysql::results result;

        // Expand the query and execute it. The expansion happens client-side.
        // If employee_name is "John", the executed query would be:
        // "SELECT id, salary FROM employee WHERE last_name = 'John'"
        co_await conn.async_execute(
            mysql::with_params("SELECT id, salary FROM employee WHERE last_name = {}", employee_name),
            result
        );
        //]
    }

    {
        //[text_queries_with_params_scalars
        // Will execute "SELECT id FROM employee WHERE salary > 42000"
        mysql::results result;
        co_await conn.async_execute(
            mysql::with_params("SELECT id FROM employee WHERE salary > {}", 42000),
            result
        );
        //]
    }

    {
        //[text_queries_with_params_optionals
        std::optional<std::int64_t> salary;  // get salary from a possibly untrusted source
        mysql::results result;

        // Depending on whether salary has a value or not, executes:
        // "UPDATE employee SET salary = 42000 WHERE id = 1"
        // "UPDATE employee SET salary = NULL WHERE id = 1"
        co_await conn.async_execute(
            mysql::with_params("UPDATE employee SET salary = {} WHERE id = {}", salary, 1),
            result
        );
        //]
    }

    {
        //[text_queries_with_params_ranges
        mysql::results result;
        std::vector<long> ids{1, 5, 20};

        // Executes "SELECT * FROM employee WHERE id IN (1, 5, 20)"
        // std::ref saves a copy
        co_await conn.async_execute(
            mysql::with_params("SELECT * FROM employee WHERE id IN ({})", std::ref(ids)),
            result
        );
        //]
    }

    {
        //[text_queries_with_params_manual_indices
        // Recall that you need to set connect_params::multi_queries to true when connecting
        // before running semicolon-separated queries. Executes:
        // "UPDATE employee SET first_name = 'John' WHERE id = 42; SELECT * FROM employee WHERE id = 42"
        mysql::results result;
        co_await conn.async_execute(
            mysql::with_params(
                "UPDATE employee SET first_name = {1} WHERE id = {0}; SELECT * FROM employee WHERE id = {0}",
                42,
                "John"
            ),
            result
        );
        //]
    }

    {
        //[text_queries_with_params_invalid_encoding
        try
        {
            // If the connection is using UTF-8 (the default), this will throw an error,
            // because the string to be formatted is not valid UTF-8.
            // The query never reaches the server.
            mysql::results result;
            co_await conn.async_execute(mysql::with_params("SELECT {}", "bad\xff UTF-8"), result);
            //<-
            BOOST_TEST(false);
            //->
        }
        catch (const boost::system::system_error& err)
        {
            BOOST_TEST(err.code() == mysql::client_errc::invalid_encoding);
        }
        //]
    }

    {
        try
        {
            //[text_queries_with_params_empty_ranges
            // If ids.empty(), generates "SELECT * FROM employee WHERE id IN ()", which is a syntax error.
            // This is not a security issue for this query, but may be exploitable in more involved scenarios.
            // Queries involving only scalar values (as opposed to ranges) are not affected by this.
            // It is your responsibility to check for conditions like ids.empty(), as client-side SQL
            // formatting does not understand your queries.
            std::vector<int> ids;
            mysql::results r;
            co_await conn.async_execute(
                mysql::with_params("SELECT * FROM employee WHERE id IN ({})", ids),
                r
            );
            //]
            BOOST_TEST(false);
        }
        catch (const mysql::error_with_diagnostics&)
        {
            // The exact error code may vary
        }
    }
}

BOOST_FIXTURE_TEST_CASE(section_text_queries, snippets_fixture)
{
    run_coro(ctx, [this] { return section_main(conn); });
}

}  // namespace

#endif
