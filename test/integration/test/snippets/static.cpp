//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/results.hpp>
#include <boost/mysql/static_results.hpp>

#include <boost/config.hpp>
#include <boost/test/unit_test.hpp>

#include "test_integration/snippets/describe.hpp"
#include "test_integration/snippets/get_connection.hpp"

#ifndef BOOST_NO_CXX17_HDR_OPTIONAL
#include <optional>
#endif

using namespace boost::mysql;

namespace {

//[describe_statistics
struct statistics
{
    std::string company;
    double average;
    double max_value;
};
BOOST_DESCRIBE_STRUCT(statistics, (), (company, average, max_value))
//]

#ifndef BOOST_NO_CXX17_HDR_OPTIONAL
//[describe_post_v2
struct post_v2
{
    int id;
    std::string title;
    std::optional<std::string> body;  // body may be NULL
};
BOOST_DESCRIBE_STRUCT(post_v2, (), (id, title, body))
//]
#endif

BOOST_AUTO_TEST_CASE(section_static)
{
#ifdef BOOST_MYSQL_CXX14

    using test::post;
    auto& conn = test::get_connection();

    {
        //[static_setup
        const char* table_definition = R"%(
            CREATE TEMPORARY TABLE posts (
                id INT PRIMARY KEY AUTO_INCREMENT,
                title VARCHAR (256) NOT NULL,
                body TEXT NOT NULL
            )
        )%";
        const char* query = "SELECT id, title, body FROM posts";
        //]

        results r;
        conn.execute(table_definition, r);

        //[static_query
        static_results<post> result;
        conn.execute(query, result);

        for (const post& p : result.rows())
        {
            // Process the post as required
            std::cout << "Title: " << p.title << "\n" << p.body << "\n";
        }
        //]

        conn.execute("DROP TABLE posts", r);
    }
    {
        //[static_field_order
        // Summing 0e0 is MySQL way to cast a DECIMAL field to DOUBLE
        const char* sql = R"%(
            SELECT
                IFNULL(AVG(salary), 0.0) + 0e0 AS average,
                IFNULL(MAX(salary), 0.0) + 0e0 AS max_value,
                company_id AS company
            FROM employee
            GROUP BY company_id
        )%";

        static_results<statistics> result;
        conn.execute(sql, result);
        //]
    }
    {
        //[static_tuples
        static_results<std::tuple<std::int64_t>> result;
        conn.execute("SELECT COUNT(*) FROM employee", result);
        std::cout << "Number of employees: " << std::get<0>(result.rows()[0]) << "\n";
        //]
    }
#ifndef BOOST_NO_CXX17_HDR_OPTIONAL
    {
        //[static_nulls_table
        const char* table_definition = R"%(
            CREATE TEMPORARY TABLE posts_v2 (
                id INT PRIMARY KEY AUTO_INCREMENT,
                title VARCHAR (256) NOT NULL,
                body TEXT
            )
        )%";
        //]

        // Verify that post_v2's definition is correct
        results r;
        conn.execute(table_definition, r);
        static_results<post_v2> result;
        conn.execute("SELECT * FROM posts_v2", result);
        conn.execute("DROP TABLE posts_v2", r);
    }
#endif  // BOOST_NO_CXX17_HDR_OPTIONAL
#endif  // BOOST_MYSQL_CXX14
}

}  // namespace