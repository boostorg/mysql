//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "boost/mysql/mysql.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/system/system_error.hpp>
#include <iostream>

/**
 * For this example, we will be using the 'boost_mysql_examples' database.
 * You can get this database by running db_setup.sql.
 * This example assumes you are connecting to a localhost MySQL server.
 *
 * This example shows how to use the metadata contained in a resultset.
 *
 * This example assumes you are already familiar with the basic concepts
 * of mysql-asio (tcp_connection, resultset, rows, values). If you are not,
 * please have a look to the query_sync.cpp example.
 */

void main_impl(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <username> <password>\n";
        exit(1);
    }

    // Connection params (host, port, user, password, database)
    boost::asio::ip::tcp::endpoint ep (boost::asio::ip::address_v4::loopback(), boost::mysql::default_port);
    boost::mysql::connection_params params (argv[1], argv[2], "boost_mysql_examples");

    // TCP and MySQL level connect
    boost::asio::io_context ctx;
    boost::mysql::tcp_connection conn (ctx);
    conn.connect(ep, params);

    // Issue the query
    const char* sql = R"(
        SELECT comp.name AS company_name, emp.id AS employee_id
        FROM employee emp
        JOIN company comp ON (comp.id = emp.company_id)
    )";
    boost::mysql::tcp_resultset result = conn.query(sql);

    /**
     * Resultsets allow you to access metadata about the fields in the query
     * using the fields() function, which returns a vector of field_metadata
     * (one per field in the query, and in the same order as in the query).
     * You can retrieve the field name, type, number of decimals,
     * suggested display width, whether the field is part of a key...
     */
    assert(result.fields().size() == 2);

    [[maybe_unused]] const boost::mysql::field_metadata& company_name = result.fields()[0];
    assert(company_name.database() == "boost_mysql_examples"); // database name
    assert(company_name.table() == "comp");                    // the alias we assigned to the table in the query
    assert(company_name.original_table() == "company");        // the original table name
    assert(company_name.field_name() == "company_name");       // the name of the field in the query
    assert(company_name.original_field_name() == "name");      // the name of the physical field in the table
    assert(company_name.type() == boost::mysql::field_type::varchar); // we created the field as a VARCHAR
    assert(!company_name.is_primary_key());                    // field is not a primary key
    assert(!company_name.is_auto_increment());                 // field is not AUTO_INCREMENT
    assert(company_name.is_not_null());                        // field may not be NULL

    [[maybe_unused]] const boost::mysql::field_metadata& employee_id = result.fields()[1];
    assert(employee_id.database() == "boost_mysql_examples"); // database name
    assert(employee_id.table() == "emp");                   // the alias we assigned to the table in the query
    assert(employee_id.original_table() == "employee");     // the original table name
    assert(employee_id.field_name() == "employee_id");      // the name of the field in the query
    assert(employee_id.original_field_name() == "id");      // the name of the physical field in the table
    assert(employee_id.type() == boost::mysql::field_type::int_);  // we created the field as INT
    assert(employee_id.is_primary_key());                   // field is a primary key
    assert(employee_id.is_auto_increment());                // we declared the field as AUTO_INCREMENT
    assert(employee_id.is_not_null());                      // field cannot be NULL

    // Close the connection
    conn.close();
}

int main(int argc, char** argv)
{
    try
    {
        main_impl(argc, argv);
    }
    catch (const boost::system::system_error& err)
    {
        std::cerr << "Error: " << err.what() << ", error code: " << err.code() << std::endl;
        return 1;
    }
    catch (const std::exception& err)
    {
        std::cerr << "Error: " << err.what() << std::endl;
        return 1;
    }
}



