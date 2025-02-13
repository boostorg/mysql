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
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <string>

#include "test_common/network_result.hpp"
#include "test_integration/run_coro.hpp"
#include "test_integration/snippets/snippets_fixture.hpp"

namespace asio = boost::asio;
namespace mysql = boost::mysql;
using mysql::string_view;
using namespace boost::mysql::test;

namespace {

asio::awaitable<void> section_main(mysql::any_connection& conn)
{
    {
        //[dynamic_views
        // Populate a results object
        mysql::results result;
        co_await conn.async_execute("SELECT 'Hello world'", result);

        // results::rows() returns a rows_view. The underlying memory is owned by the results object
        mysql::rows_view all_rows = result.rows();

        // Indexing a rows_view yields a row_view. The underlying memory is owned by the results object
        mysql::row_view first_row = all_rows.at(0);

        // Indexing a row_view yields a field_view. The underlying memory is owned by the results object
        mysql::field_view first_field = first_row.at(0);  // Contains the string "Hello world"

        //]
        BOOST_TEST(first_field.as_string() == "Hello world");

        //[dynamic_taking_ownership
        // You may use all_rows_owning after result has gone out of scope
        mysql::rows all_rows_owning{all_rows};

        // You may use first_row_owning after result has gone out of scope
        mysql::row first_row_owning{first_row};

        // You may use first_field_owning after result has gone out of scope
        mysql::field first_field_owning{first_field};
        //]
    }
    {
        //[dynamic_using_fields
        mysql::results result;
        co_await conn.async_execute("SELECT 'abc', 42", result);

        // Obtain a field's underlying value using the is_xxx and get_xxx accessors
        mysql::field_view f = result.rows().at(0).at(0);  // f points to the string "abc"
        if (f.is_string())
        {
            // we know it's a string, unchecked access
            string_view s = f.get_string();
            std::cout << s << std::endl;  // Use the string as required
        }
        else
        {
            // Oops, something went wrong - schema mismatch?
        }

        // Alternative: use the as_xxx accessor
        f = result.rows().at(0).at(1);
        std::int64_t value = f.as_int64();  // Checked access. Throws if f doesn't contain an int
        std::cout << value << std::endl;    // Use the int as required

        //]
    }
    {
        //[dynamic_handling_nulls
        mysql::results result;

        // Create some test data
        co_await conn.async_execute(
            R"%(
                CREATE TEMPORARY TABLE products (
                    id VARCHAR(50) PRIMARY KEY,
                    description VARCHAR(256)
                );
                INSERT INTO products VALUES ('PTT', 'Potatoes'), ('CAR', NULL)
            )%",
            result
        );

        // Retrieve the data. Note that some fields are NULL
        co_await conn.async_execute("SELECT id, description FROM products", result);

        for (mysql::row_view r : result.rows())
        {
            mysql::field_view description_fv = r.at(1);
            if (description_fv.is_null())
            {
                // Handle the NULL value
                // Note: description_fv.is_string() will return false here; NULL is represented as a separate
                // type
                std::cout << "No description for product_id " << r.at(0) << std::endl;
            }
            else
            {
                // Handle the non-NULL case. Get the underlying value and use it as you want
                // If there is any schema mismatch (and description was not defined as VARCHAR), this will
                // throw
                string_view description = description_fv.as_string();

                // Use description as required
                std::cout << "product_id " << r.at(0) << ": " << description << std::endl;
            }
        }
        //]

        conn.async_execute("DROP TABLE products", result, as_netresult).validate_no_error();
    }
    {
        //[dynamic_field_accessor_references
        mysql::field f("my_string");     // constructs a field that owns the string "my_string"
        std::string& s = f.as_string();  // s points into f's storage
        s.push_back('2');                // f now holds "my_string2"

        //]

        BOOST_TEST(s == "my_string2");
    }
    {
        //[dynamic_field_assignment
        mysql::field f("my_string");  // constructs a field that owns the string "my_string"
        f = 42;                       // destroys "my_string" and stores the value 42 as an int64

        //]

        BOOST_TEST(f.as_int64() == 42);
    }
}

BOOST_FIXTURE_TEST_CASE(section_dynamic, snippets_fixture)
{
    run_coro(ctx, [&] { return section_main(conn); });
}

}  // namespace

#endif
