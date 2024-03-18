//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/character_set.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/format_sql.hpp>
#include <boost/mysql/metadata_mode.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/optional/optional.hpp>
#include <boost/system/result.hpp>
#include <boost/test/unit_test.hpp>

#include <cassert>
#include <cstdint>
#include <string>
#include <utility>

#include "test_common/ci_server.hpp"
#include "test_common/printing.hpp"
#include "test_integration/snippets/credentials.hpp"

#ifndef BOOST_NO_CXX17_HDR_OPTIONAL
#include <optional>
#endif
#ifdef __cpp_lib_polymorphic_allocator
#include <memory_resource>
#endif

using namespace boost::mysql;
using namespace boost::mysql::test;

namespace {

static std::string get_name() { return "John"; }

#ifndef BOOST_NO_CXX17_HDR_OPTIONAL
//[sql_formatting_incremental_fn
// Compose an update query that sets first_name, last_name, or both
std::string compose_update_query(
    boost::mysql::format_options opts,
    std::int64_t employee_id,
    std::optional<std::string> new_first_name,
    std::optional<std::string> new_last_name
)
{
    // There should be at least one update
    assert(new_first_name || new_last_name);

    // format_context will accumulate the query as we compose it
    boost::mysql::format_context ctx(opts);

    // append_raw adds raw SQL to the generated query, without quoting or escaping.
    // You can only pass strings known at compile-time to append_raw,
    // unless you use the runtime function.
    ctx.append_raw("UPDATE employee SET ");

    if (new_first_name)
    {
        // format_sql_to expands a format string and appends the result
        // to a format context. This way, we can build our query in small pieces
        // Add the first_name update clause
        boost::mysql::format_sql_to(ctx, "first_name = {}", *new_first_name);
    }
    if (new_last_name)
    {
        if (new_first_name)
            ctx.append_raw(", ");

        // Add the last_name update clause
        boost::mysql::format_sql_to(ctx, "last_name = {}", *new_last_name);
    }

    // Add the where clause
    boost::mysql::format_sql_to(ctx, " WHERE id = {}", employee_id);

    // Retrieve the generated query string
    return std::move(ctx).get().value();
}
//]
#endif

}  // namespace

//[sql_formatting_formatter_specialization
// We want to add formatting support for employee_t
struct employee_t
{
    std::string first_name;
    std::string last_name;
    std::string company_id;
};

namespace boost {
namespace mysql {

template <>
struct formatter<employee_t>
{
    // formatter<T> should define, at least, a function with signature:
    //    static void format(const T&, format_context_base&)
    // This function must use format_sql_to, format_context_base::append_raw
    // or format_context_base::append_value to format the passed value.
    // We will make this suitable for INSERT statements
    static void format(const employee_t& emp, format_context_base& ctx)
    {
        format_sql_to(ctx, "{}, {}, {}", emp.first_name, emp.last_name, emp.company_id);
    }
};

}  // namespace mysql
}  // namespace boost
//]

namespace {

#ifndef BOOST_NO_CXX17_HDR_OPTIONAL
//[sql_formatting_unit_test
// For reference, the function under test
std::string compose_update_query(
    boost::mysql::format_options opts,
    std::int64_t employee_id,
    std::optional<std::string> new_first_name,
    std::optional<std::string> new_last_name
);

// Your test body
void test_compose_update_query()
{
    // You can safely use these format_options for testing,
    // since they are the most common ones.
    boost::mysql::format_options opts{boost::mysql::utf8mb4_charset, true};

    // Test for the different cases
    BOOST_TEST(
        compose_update_query(opts, 42, "Bob", {}) == "UPDATE employee SET first_name = 'Bob' WHERE id = 42"
    );
    BOOST_TEST(
        compose_update_query(opts, 42, {}, "Alice") == "UPDATE employee SET last_name = 'Alice' WHERE id = 42"
    );
    BOOST_TEST(
        compose_update_query(opts, 0, "Bob", "Alice") ==
        "UPDATE employee SET first_name = 'Bob', last_name = 'Alice' WHERE id = 0"
    );
}
//]
#endif

BOOST_AUTO_TEST_CASE(section_sql_formatting)
{
    auto server_hostname = get_hostname();

    connect_params params;
    params.server_address.emplace_host_and_port(server_hostname);
    params.username = mysql_username;
    params.password = mysql_password;
    params.database = "boost_mysql_examples";
    params.multi_queries = true;
    params.ssl = ssl_mode::disable;

    boost::asio::io_context ioc;
    any_connection conn(ioc);
    results r;

    conn.connect(params);

    {
        //[sql_formatting_simple
        std::string employee_name = get_name();  // employee_name is an untrusted string

        // Compose the SQL query in the client
        std::string query = boost::mysql::format_sql(
            conn.format_opts().value(),
            "SELECT id, salary FROM employee WHERE last_name = {}",
            employee_name
        );

        // If employee_name is "John", query now contains:
        // "SELECT id, salary FROM employee WHERE last_name = 'John'"
        // If employee_name contains quotes, they will be escaped as required

        // Execute the generated query as usual
        results result;
        conn.execute(query, result);
        //]

        BOOST_TEST(query == "SELECT id, salary FROM employee WHERE last_name = 'John'");
    }
    {
        //[sql_formatting_other_scalars
        std::string query = boost::mysql::format_sql(
            conn.format_opts().value(),
            "SELECT id FROM employee WHERE salary > {}",
            42000
        );

        BOOST_TEST(query == "SELECT id FROM employee WHERE salary > 42000");
        //]

        conn.execute(query, r);
    }
#ifndef BOOST_NO_CXX17_HDR_OPTIONAL
    {
        //[sql_formatting_optionals
        std::optional<std::int64_t> salary;  // get salary from a possibly untrusted source

        std::string query = boost::mysql::format_sql(
            conn.format_opts().value(),
            "UPDATE employee SET salary = {} WHERE id = {}",
            salary,
            1
        );

        // Depending on whether salary has a value or not, generates:
        // UPDATE employee SET salary = 42000 WHERE id = 1
        // UPDATE employee SET salary = NULL WHERE id = 1
        //]

        BOOST_TEST(query == "UPDATE employee SET salary = NULL WHERE id = 1");
        conn.execute(query, r);
    }
#endif
    {
        //[sql_formatting_manual_indices
        // Recall that you need to set connect_params::multi_queries to true when connecting
        // before running semicolon-separated queries.
        std::string query = boost::mysql::format_sql(
            conn.format_opts().value(),
            "UPDATE employee SET first_name = {1} WHERE id = {0}; SELECT * FROM employee WHERE id = {0}",
            42,
            "John"
        );

        BOOST_TEST(
            query ==
            "UPDATE employee SET first_name = 'John' WHERE id = 42; SELECT * FROM employee WHERE id = 42"
        );
        //]

        conn.execute(query, r);
    }
    {
        // clang-format off
        //[sql_formatting_named_args
        std::string query = boost::mysql::format_sql(
            conn.format_opts().value(),
            "UPDATE employee SET first_name = {name} WHERE id = {id}; SELECT * FROM employee WHERE id = {id}",
            {
                {"id",   42    },
                {"name", "John"}
            }
        );
        //<-
        // clang-format on
        //->

        BOOST_TEST(
            query ==
            "UPDATE employee SET first_name = 'John' WHERE id = 42; SELECT * FROM employee WHERE id = 42"
        );
        //]

        conn.execute(query, r);
    }
    {
        //[sql_formatting_identifiers
        std::string query = boost::mysql::format_sql(
            conn.format_opts().value(),
            "SELECT id, last_name FROM employee ORDER BY {} DESC",
            boost::mysql::identifier("company_id")
        );

        BOOST_TEST(query == "SELECT id, last_name FROM employee ORDER BY `company_id` DESC");
        //]

        conn.execute(query, r);
    }
    {
        //[sql_formatting_qualified_identifiers
        std::string query = boost::mysql::format_sql(
            conn.format_opts().value(),
            "SELECT salary, tax_id FROM employee "
            "INNER JOIN company ON employee.company_id = company.id "
            "ORDER BY {} DESC",
            boost::mysql::identifier("company", "id")
        );
        // SELECT ... ORDER BY `company`.`id` DESC
        //]

        BOOST_TEST(
            query ==
            "SELECT salary, tax_id FROM employee "
            "INNER JOIN company ON employee.company_id = company.id "
            "ORDER BY `company`.`id` DESC"
        );
        conn.execute(query, r);
    }
#ifndef BOOST_NO_CXX17_HDR_OPTIONAL
    {
        //[sql_formatting_incremental_use
        std::string query = compose_update_query(conn.format_opts().value(), 42, "John", {});

        BOOST_TEST(query == "UPDATE employee SET first_name = 'John' WHERE id = 42");
        //]

        conn.execute(query, r);
    }
    {
        test_compose_update_query();
    }
#endif
    {
        try
        {
            //[sql_formatting_invalid_encoding
            // If the connection is using UTF-8 (the default), this will throw an error,
            // because the string to be formatted contains invalid UTF8.
            format_sql(conn.format_opts().value(), "SELECT {}", "bad\xff UTF-8");
            //]

            BOOST_TEST(false);
        }
        catch (const boost::system::system_error& err)
        {
            BOOST_TEST(err.code() == boost::mysql::client_errc::invalid_encoding);
        }
    }
    {
        using namespace boost::mysql;
        using boost::optional;
        const auto opts = conn.format_opts().value();

        //[sql_formatting_reference_signed
        //<-
        BOOST_TEST(
            //->
            format_sql(opts, "SELECT {}", 42) == "SELECT 42"
            //<-
        );
        //->
        //<-
        BOOST_TEST(
            //->
            format_sql(opts, "SELECT {}", -1) == "SELECT -1"
            //<-
        );
        //->
        //]

        //[sql_formatting_reference_unsigned
        //<-
        BOOST_TEST(
            //->
            format_sql(opts, "SELECT {}", 42u) == "SELECT 42"
            //<-
        );
        //->
        //]

        //[sql_formatting_reference_bool
        //<-
        BOOST_TEST(
            //->
            format_sql(opts, "SELECT {}", false) == "SELECT 0"
            //<-
        );
        //->
        //<-
        BOOST_TEST(
            //->
            format_sql(opts, "SELECT {}", true) == "SELECT 1"
            //<-
        );
        //->
        //]

        //[sql_formatting_reference_string
        //<-
        BOOST_TEST(
            //->
            format_sql(opts, "SELECT {}", "Hello world") == "SELECT 'Hello world'"
            //<-
        );
        //->
        //<-
        BOOST_TEST(
            //->
            format_sql(opts, "SELECT {}", "Hello 'world'") == R"(SELECT 'Hello \'world\'')"
            //<-
        );
        //->
        //]

        //[sql_formatting_reference_blob
        //<-
        BOOST_TEST(
            //->
            format_sql(opts, "SELECT {}", blob{0x00, 0x48, 0xff}) == R"(SELECT x'0048ff')"
            //<-
        );
        //->
        //]

        //[sql_formatting_reference_float
        //<-
        BOOST_TEST(
            //->
            // Equivalent to format_sql(opts, "SELECT {}", static_cast<double>(4.2f))
            // Note that MySQL uses doubles for all floating point literals
            format_sql(opts, "SELECT {}", 4.2f) == "SELECT 4.199999809265137e+00"
            //<-
        );
        //->
        //]

        //[sql_formatting_reference_double
        //<-
        BOOST_TEST(
            //->
            format_sql(opts, "SELECT {}", 4.2) == "SELECT 4.2e+00"
            //<-
        );
        //->
        //]

        //[sql_formatting_reference_date
        //<-
        BOOST_TEST(
            //->
            format_sql(opts, "SELECT {}", date(2021, 1, 2)) == "SELECT '2021-01-02'"
            //<-
        );
        //->
        //]

        //[sql_formatting_reference_datetime
        //<-
        BOOST_TEST(
            // clang-format off
            //->
            format_sql(opts, "SELECT {}", datetime(2021, 1, 2, 23, 51, 14)) == "SELECT '2021-01-02 23:51:14.000000'"
            //<-
            // clang-format on
        );
        //->
        //]

        //[sql_formatting_reference_time
        //<-
        BOOST_TEST(
            //->
            format_sql(opts, "SELECT {}", std::chrono::seconds(121)) == "SELECT '00:02:01.000000'"
            //<-
        );
        //->
        //]

        //[sql_formatting_reference_nullptr
        //<-
        BOOST_TEST(
            //->
            format_sql(opts, "SELECT {}", nullptr) == "SELECT NULL"
            //<-
        );
        //->
        //]

        //[sql_formatting_reference_optional
        //<-
        BOOST_TEST(
            //->
            format_sql(opts, "SELECT {}", optional<int>(42)) == "SELECT 42"
            //<-
        );
        //->
        //<-
        BOOST_TEST(
            //->
            format_sql(opts, "SELECT {}", optional<int>()) == "SELECT NULL"
            //<-
        );
        //->
        //]

        //[sql_formatting_reference_field
        //<-
        BOOST_TEST(
            //->
            format_sql(opts, "SELECT {}", field(42)) == "SELECT 42"
            //<-
        );
        //->
        //<-
        BOOST_TEST(
            //->
            format_sql(opts, "SELECT {}", field("abc")) == "SELECT 'abc'"
            //<-
        );
        //->
        //<-
        BOOST_TEST(
            //->
            format_sql(opts, "SELECT {}", field()) == "SELECT NULL"
            //<-
        );
        //->
        //]

        //[sql_formatting_reference_identifier
        //<-
        BOOST_TEST(
            //->
            format_sql(opts, "SELECT {} FROM t", identifier("salary")) == "SELECT `salary` FROM t"
            //<-
        );
        //->
        //<-
        BOOST_TEST(
            //->
            format_sql(opts, "SELECT {} FROM t", identifier("sal`ary")) == "SELECT `sal``ary` FROM t"
            //<-
        );
        //->
        //<-
        BOOST_TEST(
            // clang-format off
            //->
            format_sql(opts, "SELECT {} FROM t", identifier("mytable", "myfield")) == "SELECT `mytable`.`myfield` FROM t"
            //<-
            // clang-format on
        );
        //->
        //<-
        BOOST_TEST(
            // clang-format off
            //->
            format_sql(opts, "SELECT {} FROM t", identifier("mydb", "mytable", "myfield")) == "SELECT `mydb`.`mytable`.`myfield` FROM t"
            //<-
            // clang-format on
        );
        //->
        //]
    }

    // Advanced section
    {
        //[sql_formatting_formatter_use
        // We can now use employee as a built-in value
        std::string query = boost::mysql::format_sql(
            conn.format_opts().value(),
            "INSERT INTO employee (first_name, last_name, company_id) VALUES ({}), ({})",
            employee_t{"John", "Doe", "HGS"},
            employee_t{"Rick", "Johnson", "AWC"}
        );

        BOOST_TEST(
            query ==
            "INSERT INTO employee (first_name, last_name, company_id) VALUES "
            "('John', 'Doe', 'HGS'), ('Rick', 'Johnson', 'AWC')"
        );
        //]

        conn.execute(query, r);
    }
    {
        const auto opts = conn.format_opts().value();

        //[sql_formatting_auto_indexing
        BOOST_TEST(format_sql(opts, "SELECT {}, {}, {}", 42, "abc", nullptr) == "SELECT 42, 'abc', NULL");
        //]
    }
    {
        const auto opts = conn.format_opts().value();

        //[sql_formatting_manual_auto_mix
        try
        {
            // Mixing manual and auto indexing is illegal. This will throw an exception.
            format_sql(opts, "SELECT {0}, {}", 42);
            //<-
            BOOST_TEST(false);
            //->
        }
        catch (const boost::system::system_error& err)
        {
            BOOST_TEST(err.code() == boost::mysql::client_errc::format_string_manual_auto_mix);
        }
        //]
    }
    {
        const auto opts = conn.format_opts().value();

        //[sql_formatting_unused_args
        // This is OK
        std::string query = format_sql(opts, "SELECT {}", 42, "abc");
        //]
        BOOST_TEST(query == "SELECT 42");
    }
    {
        const auto opts = conn.format_opts().value();

        //[sql_formatting_brace_literal
        BOOST_TEST(
            format_sql(opts, "SELECT 'Brace literals: {{ and }}'") == "SELECT 'Brace literals: { and }'"
        );
        //]
    }
    {
        const auto opts = conn.format_opts().value();

        //[sql_formatting_format_double_error
        try
        {
            // We're trying to format a double infinity value, which is not
            // supported by MySQL. This will throw an exception.
            std::string formatted_query = format_sql(opts, "SELECT {}", HUGE_VAL);
            //<-
            BOOST_TEST(false);
            boost::ignore_unused(formatted_query);
            //->
        }
        catch (const boost::system::system_error& err)
        {
            BOOST_TEST(err.code() == boost::mysql::client_errc::unformattable_value);
        }
        //]
    }
    {
        const auto opts = conn.format_opts().value();

        //[sql_formatting_no_exceptions
        // ctx contains an error code that tracks whether any error happened
        boost::mysql::format_context ctx(opts);

        // We're trying to format a infinity, which is an error. This
        // will set the error state, but won't throw.
        format_sql_to(ctx, "SELECT {}, {}", HUGE_VAL, 42);

        // The error state gets checked at this point. Since it is set,
        // res will contain an error.
        boost::system::result<std::string> res = std::move(ctx).get();
        BOOST_TEST(!res.has_value());
        BOOST_TEST(res.has_error());
        BOOST_TEST(res.error() == boost::mysql::client_errc::unformattable_value);
        // res.value() would throw an error, like format_sql would
        //]
    }
#ifdef __cpp_lib_polymorphic_allocator
    {
        //[sql_formatting_custom_string
        // Create a format context that uses std::pmr::string
        boost::mysql::basic_format_context<std::pmr::string> ctx(conn.format_opts().value());

        // Compose your query as usual
        boost::mysql::format_sql_to(ctx, "SELECT * FROM employee WHERE id = {}", 42);

        // Retrieve the query as usual
        std::pmr::string query = std::move(ctx).get().value();
        //]

        BOOST_TEST(query == "SELECT * FROM employee WHERE id = 42");
        conn.execute(query, r);
    }
#endif
    {
        //[sql_formatting_memory_reuse
        // we want to re-use memory held by storage
        std::string storage;

        // storage is moved into ctx by the constructor. If any memory
        // had been allocated by the string, it will be re-used.
        boost::mysql::format_context ctx(conn.format_opts().value(), std::move(storage));

        // Use ctx as you normally would
        boost::mysql::format_sql_to(ctx, "SELECT {}", 42);

        // When calling get(), the string is moved out of the context
        std::string query = std::move(ctx).get().value();
        //]

        BOOST_TEST(query == "SELECT 42");
    }
}

}  // namespace
