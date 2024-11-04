//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/results.hpp>
#include <boost/mysql/row_view.hpp>

#include <boost/test/unit_test.hpp>

#include "test_integration/snippets/snippets_fixture.hpp"

namespace mysql = boost::mysql;
using namespace mysql::test;

namespace {

// Taken here because it's only used in the discussion
void print_employee(mysql::string_view first_name, mysql::string_view last_name)
{
    std::cout << "Employee's name is: " << first_name << ' ' << last_name << std::endl;
}

BOOST_FIXTURE_TEST_CASE(section_tutorials, snippets_fixture)
{
    {
        mysql::results result;
        conn.execute("SELECT first_name, last_name FROM employee WHERE id = 1", result);

        //[tutorial_static_casts
        mysql::row_view employee = result.rows().at(0);
        print_employee(employee.at(0).as_string(), employee.at(1).as_string());
        //]
    }
}

}  // namespace
