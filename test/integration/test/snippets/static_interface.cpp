//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/static_results.hpp>
#ifdef BOOST_MYSQL_CXX14

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/pfr.hpp>
#include <boost/mysql/results.hpp>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/config.hpp>
#include <boost/describe/class.hpp>
#include <boost/test/unit_test.hpp>

#include <cstdint>
#include <string>
#include <tuple>
#ifndef BOOST_NO_CXX17_HDR_OPTIONAL
#include <optional>
#endif

#include "test_common/network_result.hpp"
#include "test_common/printing.hpp"
#include "test_integration/run_coro.hpp"
#include "test_integration/snippets/snippets_fixture.hpp"

namespace asio = boost::asio;
namespace mysql = boost::mysql;
using namespace mysql::test;

// PFR types can't be placed in anonymous namespaces
namespace snippets_static {

//
// Main explanation. These snippets require C++20
//
#ifdef BOOST_ASIO_HAS_CO_AWAIT

//[static_interface_describe_employee_v1
// We can use a plain struct with ints and strings to describe our rows.
struct employee_v1
{
    int id;
    std::string first_name;
    std::string last_name;
};

// This must be placed at the namespace level. It adds reflection capabilities to our struct
BOOST_DESCRIBE_STRUCT(employee_v1, (), (id, first_name, last_name))
//]

//[static_interface_describe_statistics
struct statistics
{
    std::string company;
    double average;
    double max_value;
};
BOOST_DESCRIBE_STRUCT(statistics, (), (company, average, max_value))
//]

//[static_interface_describe_employee_v2
// If we try to query the employee table with this struct definition,
// an error will be issued because salary can be NULL in the database,
// but not in the C++ type
struct employee_v2
{
    int id;
    std::string first_name;
    std::string last_name;
    unsigned salary;
};
BOOST_DESCRIBE_STRUCT(employee_v2, (), (id, first_name, last_name, salary))
//]

//[static_interface_describe_employee_v3
struct employee_v3
{
    int id;
    std::string first_name;
    std::string last_name;
    std::optional<unsigned> salary;  // salary might be NULL in the database
};
BOOST_DESCRIBE_STRUCT(employee_v3, (), (id, first_name, last_name, salary))
//]

//[static_interface_pfr_employee
// employee_v4 doesn't contain any metadata - we're not using BOOST_DESCRIBE_STRUCT here
struct employee_v4
{
    int id;
    std::string first_name;
    std::string last_name;
    std::optional<unsigned> salary;
};
//]

asio::awaitable<void> section_main(mysql::any_connection& conn)
{
    {
        //[static_interface_query
        mysql::static_results<employee_v1> result;
        co_await conn.async_execute("SELECT id, first_name, last_name FROM employee LIMIT 10", result);

        for (const employee_v1& emp : result.rows())
        {
            // Process the employee as required
            std::cout << "ID: " << emp.id << ": " << emp.first_name << ' ' << emp.last_name << "\n";
        }
        //]
    }
    {
        //[static_interface_field_order
        // Summing 0e0 is MySQL way to cast a DECIMAL field to DOUBLE
        constexpr const char* sql = R"%(
            SELECT
                IFNULL(AVG(salary), 0.0) + 0e0 AS average,
                IFNULL(MAX(salary), 0.0) + 0e0 AS max_value,
                company_id AS company
            FROM employee
            GROUP BY company_id
        )%";

        mysql::static_results<statistics> result;
        co_await conn.async_execute(sql, result);
        //]
    }
    {
        // Check that the optional version works
        mysql::static_results<employee_v3> result;
        co_await conn.async_execute("SELECT * FROM employee LIMIT 1", result);
    }
#if BOOST_PFR_CORE_NAME_ENABLED
    {
        //[static_interface_pfr_by_name
        // pfr_by_name is a marker type. It tells static_results to use
        // Boost.PFR for reflection, instead of Boost.Describe.
        mysql::static_results<mysql::pfr_by_name<employee_v4>> result;

        // As with Boost.Describe, query fields are matched to struct
        // members by name. This means that the fields in the query
        // may appear in any order.
        co_await conn.async_execute("SELECT * FROM employee LIMIT 10", result);

        // Note that result.rows() is a span of employee_v4 objects,
        // rather than pfr_by_name<employee_v4> objects. employee_v4
        // is the underlying row type for pfr_by_name<employee_v4>
        for (const employee_v4& emp : result.rows())
        {
            // Process the employee as required
            std::cout << "ID: " << emp.id << ": " << emp.first_name << ' ' << emp.last_name << "\n";
        }
        //]
    }
    {
        //[static_interface_pfr_by_position
        // pfr_by_position is another marker type.
        // Fields in employee_v4 must appear in the same order as in the query,
        // as matching will be done by position.
        mysql::static_results<mysql::pfr_by_position<employee_v4>> result;
        co_await conn.async_execute("SELECT id, first_name, last_name, salary FROM employee", result);

        // The underlying row type is employee_v4
        for (const employee_v4& emp : result.rows())
        {
            // Process the employee as required
            std::cout << "ID: " << emp.id << ": " << emp.first_name << ' ' << emp.last_name << "\n";
        }
        //]
    }
#endif
    {
        //[static_interface_tuples
        mysql::static_results<std::tuple<std::int64_t>> result;
        co_await conn.async_execute("SELECT COUNT(*) FROM employee", result);
        std::cout << "Number of employees: " << std::get<0>(result.rows()[0]) << "\n";
        //]
    }
}

BOOST_FIXTURE_TEST_CASE(section_static_interface, snippets_fixture)
{
    run_coro(ctx, [this] { return section_main(conn); });
}

BOOST_FIXTURE_TEST_CASE(section_static_interface_error, snippets_fixture)
{
    // Check the nullability error. At the moment, this is a fatal error,
    // so it must be run in a separate test case
    mysql::static_results<employee_v2> result;
    conn.async_execute("SELECT * FROM employee LIMIT 1", result, as_netresult)
        .validate_error(
            mysql::client_errc::metadata_check_failed,
            "NULL checks failed for field 'salary': the database type may be NULL, but the C++ type cannot. "
            "Use std::optional<T> or boost::optional<T>"
        );
}

#endif

//
// Comparison table. This part does not require C++20,
// since the table has a "C++ standard required" field.
// We want all type definitions here to be similar.
//

namespace descr_type {
//[static_interface_comparison_describe_struct
// Definition should be at namespace scope
struct employee
{
    int id;
    std::string first_name;
    std::string last_name;
};
BOOST_DESCRIBE_STRUCT(employee, (), (id, first_name, last_name))
//]
}  // namespace descr_type

namespace pfr_type {
//[static_interface_comparison_pfr_struct
// Definition should be at namespace scope
struct employee
{
    int id;
    std::string first_name;
    std::string last_name;
};
//]
}  // namespace pfr_type

BOOST_FIXTURE_TEST_CASE(section_static_interface_comparison_table, snippets_fixture)
{
    // Left as sync because the table has a "C++ standard required" field
    // that is < C++20 for most of the techniques
    {
        using namespace descr_type;
        //[static_interface_comparison_describe
        // Usage
        mysql::static_results<employee> result;
        conn.execute("SELECT first_name, last_name, id FROM employee", result);
        //]
    }
#if BOOST_PFR_CORE_NAME_ENABLED
    {
        using namespace pfr_type;
        //[static_interface_comparison_pfr_by_name
        // Usage
        mysql::static_results<mysql::pfr_by_name<employee>> result;
        conn.execute("SELECT first_name, last_name, id FROM employee", result);
        //]
    }
#endif
#if BOOST_PFR_USE_CPP17
    {
        using namespace pfr_type;
        //[static_interface_comparison_pfr_by_position
        // Usage
        mysql::static_results<mysql::pfr_by_position<employee>> result;
        conn.execute("SELECT id, first_name, last_name FROM employee", result);
        //]
    }
#endif
    {
        //[static_interface_comparison_tuples
        using tuple_t = std::tuple<int, std::string, std::string>;
        mysql::static_results<tuple_t> result;
        conn.execute("SELECT id, first_name, last_name FROM employee", result);
        //]
    }
}

}  // namespace snippets_static

#endif
