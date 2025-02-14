//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/character_set.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/format_sql.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/sequence.hpp>
#include <boost/mysql/string_view.hpp>
#include <boost/mysql/with_params.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/assert/source_location.hpp>
#include <boost/optional/optional.hpp>
#include <boost/system/result.hpp>
#include <boost/test/unit_test.hpp>

#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "test_common/has_ranges.hpp"
#include "test_common/network_result.hpp"
#include "test_common/printing.hpp"
#include "test_common/source_location.hpp"
#include "test_integration/snippets/snippets_fixture.hpp"

#ifndef BOOST_NO_CXX17_HDR_OPTIONAL
#include <optional>
#endif
#ifndef BOOST_NO_CXX17_HDR_STRING_VIEW
#include <string_view>
#endif
#ifndef BOOST_NO_CXX17_HDR_MEMORY_RESOURCE
#include <memory_resource>
#endif
#ifdef BOOST_MYSQL_HAS_RANGES
#include <ranges>
#endif

namespace asio = boost::asio;
namespace mysql = boost::mysql;
using namespace mysql::test;

inline namespace sql_formatting {
//[sql_formatting_formatter_specialization
// We want to add formatting support for employee
struct employee
{
    std::string first_name;
    std::string last_name;
    std::string company_id;
};
//<-
}  // namespace sql_formatting
//->
namespace boost {
namespace mysql {

template <>
struct formatter<employee>
{
    // formatter<T> should define the following functions:
    //    const char* parse(const char* first, const char*);
    //    void format(const T&, format_context_base&) const;

    const char* parse(const char* begin, const char* /* end */)
    {
        // Parse any format specifiers for this type.
        // [begin, end) points to the range of characters holding the format specifier string
        // We should return a pointer to the first unparsed character.
        // We don't support any specifiers for this type, so we return the begin pointer.
        return begin;
    }

    void format(const employee& emp, format_context_base& ctx) const
    {
        // Perform the actual formatting by appending characters to ctx.
        // We usually use format_sql_to to achieve this.
        // We will make this suitable for INSERT statements
        format_sql_to(ctx, "{}, {}, {}", emp.first_name, emp.last_name, emp.company_id);
    }
};

}  // namespace mysql
}  // namespace boost
//]

namespace {

// Used to verify that the generated query is well-formed
void run_query(
    mysql::any_connection& conn,
    mysql::string_view q,
    boost::source_location loc = BOOST_MYSQL_CURRENT_LOCATION
)
{
    mysql::results r;
    conn.async_execute(q, r, as_netresult).validate_no_error(loc);
}

#if !defined(BOOST_NO_CXX17_HDR_OPTIONAL) && !defined(BOOST_NO_CXX17_HDR_STRING_VIEW)
//[sql_formatting_incremental_fn
// Compose a query that retrieves all employees in a company,
// with an optional limit
std::string compose_select_query(
    mysql::format_options opts,
    std::string_view company_id,
    std::optional<long> limit
)
{
    // format_context will accumulate the query as we compose it
    mysql::format_context ctx(opts);

    // format_sql_to expands a format string and appends the result
    // to a format context. This way, we can build our query in smaller pieces
    // Add all the query except for the LIMIT clause
    mysql::format_sql_to(ctx, "SELECT * FROM employee WHERE company_id = {}", company_id);

    if (limit)
    {
        // Add the LIMIT clause
        mysql::format_sql_to(ctx, " LIMIT {}", *limit);
    }

    // Retrieve the generated query string.
    // get() returns a boost::system::result<std::string> that
    // contains an error if any of the format operations failed.
    // Calling value() will throw on error, like format_sql does
    return std::move(ctx).get().value();
}
//]
#endif

BOOST_AUTO_TEST_CASE(section_sql_formatting)
{
    snippets_fixture fix;  // Prevent name shadowing warnings under MSVC with ctx
    auto& conn = fix.conn;
    const auto opts = conn.format_opts().value();

    {
        //[sql_formatting_format_sql
        // Compose the SQL query without executing it.
        // format_opts returns a system::result<format_options>,
        // contains settings like the current character set.
        // If the connection is using an unknown character set, this will throw an error.
        std::string query = mysql::format_sql(
            conn.format_opts().value(),
            "SELECT id, salary FROM employee WHERE last_name = {}",
            "Doe"
        );

        BOOST_TEST(query == "SELECT id, salary FROM employee WHERE last_name = 'Doe'");
        //]

        run_query(conn, query);
    }
#if !defined(BOOST_NO_CXX17_HDR_OPTIONAL) && !defined(BOOST_NO_CXX17_HDR_STRING_VIEW)
    {
        //[sql_formatting_incremental_use
        std::string query = compose_select_query(conn.format_opts().value(), "HGS", {});
        BOOST_TEST(query == "SELECT * FROM employee WHERE company_id = 'HGS'");
        //<-
        run_query(conn, query);
        //->

        query = compose_select_query(conn.format_opts().value(), "HGS", 50);
        BOOST_TEST(query == "SELECT * FROM employee WHERE company_id = 'HGS' LIMIT 50");
        //]

        run_query(conn, query);
    }
#endif
    {
        //[sql_formatting_sequence_1
        // Employee is a plain struct, not formattable by default
        std::vector<employee> employees{
            {"John", "Doe",   "HGS"},
            {"Kate", "Smith", "AWC"},
        };
        std::string query = format_sql(
            conn.format_opts().value(),
            "INSERT INTO employee (first_name, last_name, company_id) VALUES {}",
            mysql::sequence(
                employees,
                [](const employee& e, mysql::format_context_base& ctx) {
                    // This function will be called for each element in employees,
                    // and should format a single element into the passed ctx.
                    // Commas will be inserted separating elements.
                    format_sql_to(ctx, "({}, {}, {})", e.first_name, e.last_name, e.company_id);
                }
            )
        );
        BOOST_TEST(
            query ==
            "INSERT INTO employee (first_name, last_name, company_id) VALUES "
            "('John', 'Doe', 'HGS'), ('Kate', 'Smith', 'AWC')"
        );
        //]
        run_query(conn, query);
    }
#ifndef BOOST_NO_CXX17_HDR_STRING_VIEW
    {
        //[sql_formatting_sequence_2
        // A collection of filters to apply to a query
        std::vector<std::pair<std::string_view, std::string_view>> filters{
            {"company_id", "HGS" },
            {"first_name", "John"},
        };

        std::string query = format_sql(
            conn.format_opts().value(),
            "SELECT * FROM employee WHERE {}",
            mysql::sequence(
                filters,
                [](const std::pair<std::string_view, std::string_view>& f, mysql::format_context_base& ctx) {
                    // Compose a single filter
                    format_sql_to(ctx, "{:i} = {}", f.first, f.second);
                },
                " AND "  // glue string: separate each element with AND clauses
            )
        );

        BOOST_TEST(query == "SELECT * FROM employee WHERE `company_id` = 'HGS' AND `first_name` = 'John'");
        //]
        run_query(conn, query);
    }
#endif

    {
        //[sql_formatting_specifiers
        std::string query = mysql::format_sql(
            conn.format_opts().value(),
            "SELECT id, last_name FROM employee ORDER BY {:i} DESC",
            "company_id"
        );

        BOOST_TEST(query == "SELECT id, last_name FROM employee ORDER BY `company_id` DESC");
        //]

        run_query(conn, query);
    }
    {
        //[sql_formatting_specifiers_explicit_indices
        std::string query = mysql::format_sql(
            conn.format_opts().value(),
            "SELECT id, last_name FROM employee ORDER BY {0:i} DESC",
            "company_id"
        );
        //]

        BOOST_TEST(query == "SELECT id, last_name FROM employee ORDER BY `company_id` DESC");
        run_query(conn, query);
    }
    {
        //[sql_formatting_formatter_use
        // We can now use employee as a built-in value
        std::string query = mysql::format_sql(
            conn.format_opts().value(),
            "INSERT INTO employee (first_name, last_name, company_id) VALUES ({}), ({})",
            employee{"John", "Doe", "HGS"},
            employee{"Rick", "Johnson", "AWC"}
        );

        BOOST_TEST(
            query ==
            "INSERT INTO employee (first_name, last_name, company_id) VALUES "
            "('John', 'Doe', 'HGS'), ('Rick', 'Johnson', 'AWC')"
        );
        //]

        run_query(conn, query);
    }
    {
        //[sql_formatting_auto_indexing
        BOOST_TEST(
            mysql::format_sql(opts, "SELECT {}, {}, {}", 42, "abc", nullptr) == "SELECT 42, 'abc', NULL"
        );
        //]
    }
    {
        //[sql_formatting_manual_auto_mix
        try
        {
            // Mixing manual and auto indexing is illegal. This will throw an exception.
            mysql::format_sql(opts, "SELECT {0}, {}", 42);
            //<-
            BOOST_TEST(false);
            //->
        }
        catch (const boost::system::system_error& err)
        {
            BOOST_TEST(err.code() == mysql::client_errc::format_string_manual_auto_mix);
        }
        //]
    }
    {
        //[sql_formatting_unused_args
        // This is OK
        std::string query = mysql::format_sql(opts, "SELECT {}", 42, "abc");
        //]
        BOOST_TEST(query == "SELECT 42");
    }
    {
        //[sql_formatting_brace_literal
        BOOST_TEST(
            format_sql(opts, "SELECT 'Brace literals: {{ and }}'") == "SELECT 'Brace literals: { and }'"
        );
        //]
    }
    {
        //[sql_formatting_format_double_error
        try
        {
            // We're trying to format a double infinity value, which is not
            // supported by MySQL. This will throw an exception.
            std::string formatted_query = mysql::format_sql(opts, "SELECT {}", HUGE_VAL);
            //<-
            BOOST_TEST(false);
            boost::ignore_unused(formatted_query);
            //->
        }
        catch (const boost::system::system_error& err)
        {
            BOOST_TEST(err.code() == mysql::client_errc::unformattable_value);
        }
        //]
    }
    {
        //[sql_formatting_no_exceptions
        // ctx contains an error code that tracks whether any error happened
        mysql::format_context ctx(opts);

        // We're trying to format a infinity, which is an error. This
        // will set the error state, but won't throw.
        mysql::format_sql_to(ctx, "SELECT {}, {}", HUGE_VAL, 42);

        // The error state gets checked at this point. Since it is set,
        // res will contain an error.
        boost::system::result<std::string> res = std::move(ctx).get();
        BOOST_TEST(!res.has_value());
        BOOST_TEST(res.has_error());
        BOOST_TEST(res.error() == mysql::client_errc::unformattable_value);
        // res.value() would throw an error, like format_sql would
        //]
    }
#ifndef BOOST_NO_CXX17_HDR_MEMORY_RESOURCE
    {
        //[sql_formatting_custom_string
        // Create a format context that uses std::pmr::string
        mysql::basic_format_context<std::pmr::string> ctx(conn.format_opts().value());

        // Compose your query as usual
        mysql::format_sql_to(ctx, "SELECT * FROM employee WHERE id = {}", 42);

        // Retrieve the query as usual
        std::pmr::string query = std::move(ctx).get().value();
        //]

        BOOST_TEST(query == "SELECT * FROM employee WHERE id = 42");
        run_query(conn, query);
    }
#endif
    {
        //[sql_formatting_memory_reuse
        // we want to re-use memory held by storage
        std::string storage;

        // storage is moved into ctx by the constructor. If any memory
        // had been allocated by the string, it will be re-used.
        mysql::format_context ctx(conn.format_opts().value(), std::move(storage));

        // Use ctx as you normally would
        mysql::format_sql_to(ctx, "SELECT {}", 42);

        // When calling get(), the string is moved out of the context
        std::string query = std::move(ctx).get().value();
        //]

        BOOST_TEST(query == "SELECT 42");
    }
}

BOOST_AUTO_TEST_CASE(section_sql_formatting_reference)
{
    {
        using namespace boost::mysql;
        using boost::optional;
        const mysql::format_options opts{mysql::utf8mb4_charset, true};

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

        // clang-format off
        //[sql_formatting_reference_string
        // Without format specifier: escaped, quoted string value
        //<-
        BOOST_TEST(
        //->
        format_sql(opts, "SELECT {}", "Hello world") == "SELECT 'Hello world'"
        //<-
        );
        BOOST_TEST(
        //->
        format_sql(opts, "SELECT {}", "Hello 'world'") == R"(SELECT 'Hello \'world\'')"
        //<-
        );
        //->

        // {:i}: escaped, quoted dynamic identifier
        //<-
        BOOST_TEST(
        //->
        format_sql(opts, "SELECT {:i} FROM t", "salary") == "SELECT `salary` FROM t"
        //<-
        );
        BOOST_TEST(
        //->
        format_sql(opts, "SELECT {:i} FROM t", "sal`ary") == "SELECT `sal``ary` FROM t"
        //<-
        );
        //->

        // {:r}: raw, unescaped SQL. WARNING: incorrect use can cause vulnerabilities
        //<-
        BOOST_TEST(
        //->
        format_sql(opts, "SELECT * FROM t WHERE id = 42 {:r} salary > 20000", "OR") ==
            "SELECT * FROM t WHERE id = 42 OR salary > 20000"
        //<-
        );
        //->
        //]
        // clang-format on

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

        //[sql_formatting_reference_ranges
        //<-
        BOOST_TEST(
            //->
            // long is a WritableField
            format_sql(opts, "SELECT {}", std::vector<long>{1, 5, 20}) == "SELECT 1, 5, 20"
            //<-
        );
//->

//<-
#ifdef BOOST_MYSQL_HAS_RANGES
        BOOST_TEST(
            //->
            // C++20 ranges and other custom ranges accepted
            format_sql(opts, "SELECT {}", std::vector<long>{1, 5, 20} | std::ranges::views::take(2)) ==
            "SELECT 1, 5"
            //<-
        );
#endif
        //->

        //<-
        BOOST_TEST(
            //->
            // Apply the 'i' specifier to each element in the sequence
            format_sql(
                opts,
                "SELECT {::i} FROM employee",
                std::vector<string_view>{"first_name", "last_name"}
            ) == "SELECT `first_name`, `last_name` FROM employee"
            //<-
        );
        //->
        //]

        //[sql_formatting_reference_sequence
        //<-
        BOOST_TEST(
            //->
            format_sql(
                opts,
                "SELECT {}",
                sequence(
                    std::vector<int>{1, 5, 20},
                    [](int val, format_context_base& ctx) { format_sql_to(ctx, "{}+1", val); }
                )
            ) == "SELECT 1+1, 5+1, 20+1"
            //<-
        );
        //->
        //]

        //[sql_formatting_reference_formattable_ref
        //<-
        BOOST_TEST(
            //->
            format_sql(opts, "SELECT {}", formattable_ref(42)) == "SELECT 42"
            //<-
        );
        BOOST_TEST(
            //->
            format_sql(opts, "SELECT {:i} FROM t", formattable_ref("salary")) == "SELECT `salary` FROM t"
            //<-
        );
        //->
        //]
    }
}

}  // namespace
