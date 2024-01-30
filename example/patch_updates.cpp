//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//[example_patch_updates

// Uses client-side SQL formatting to implement a dynamic filter.
// If you're implementing a filter with many options that can be
// conditionally enabled, this pattern may be useful for you.
//
// Client-side SQL formatting is an experimental feature.

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/format_sql.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/ssl_mode.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/core/span.hpp>

#include <cstdint>
#include <iostream>
#include <string>

using boost::mysql::error_code;
using boost::mysql::field_view;
using boost::mysql::string_view;

[[noreturn]] static void usage(const char* prog_name)
{
    std::cerr << "Usage: " << prog_name << " <username> <password> <server-hostname> employee_id [updates]\n";
    exit(1);
}

struct update_field
{
    string_view field_name;
    boost::mysql::field_view field_value;
};

struct update_field_list
{
    std::vector<update_field> updates;
};

namespace boost {
namespace mysql {

template <>
struct formatter<update_field_list>
{
    static void format(const update_field_list& value, format_context_base& ctx)
    {
        assert(!value.updates.empty());
        bool is_first = true;
        for (const auto& update : value.updates)
        {
            if (!is_first)
            {
                ctx.append_raw(", ");
            }
            is_first = false;
            ctx.append_value(identifier(update.field_name))
                .append_raw(" = ")
                .append_value(update.field_value);
        }
    }
};

}  // namespace mysql
}  // namespace boost

static update_field_list parse_update_fields(const char* prog_name, boost::span<char* const> argv)
{
    // Available options
    const string_view company_id_prefix = "--company-id=";
    const string_view first_name_prefix = "--first-name=";
    const string_view last_name_prefix = "--last-name=";
    const string_view salary_prefix = "--salary=";

    // Parse the command line
    update_field_list res;
    for (string_view arg : argv)
    {
        if (arg.starts_with(company_id_prefix))
        {
            string_view new_value = arg.substr(company_id_prefix.size());
            res.updates.push_back(update_field{"company_id", field_view(new_value)});
        }
        else if (arg.starts_with(first_name_prefix))
        {
            string_view new_value = arg.substr(first_name_prefix.size());
            res.updates.push_back(update_field{"first_name", field_view(new_value)});
        }
        else if (arg.starts_with(last_name_prefix))
        {
            string_view new_value = arg.substr(last_name_prefix.size());
            res.updates.push_back(update_field{"last_name", field_view(new_value)});
        }
        else if (arg.starts_with(salary_prefix))
        {
            double new_value = std::stod(arg.substr(salary_prefix.size()));
            res.updates.push_back(update_field{"salary", field_view(new_value)});
        }
        else
        {
            std::cerr << "Unrecognized option: " << arg << std::endl;
            usage(prog_name);
        }
    }

    // We should have at least one update
    if (res.updates.empty())
    {
        std::cerr << "At least one update field should be specified" << std::endl;
        usage(prog_name);
    }

    return res;
}

void main_impl(int argc, char** argv)
{
    if (argc <= 5)
    {
        std::cerr << "Usage: " << argv[0]
                  << " <username> <password> <server-hostname> <employee_id> [filters]\n";
        exit(1);
    }

    // Parse the employee ID to be updated
    std::int64_t employee_id = std::stoll(argv[4]);

    // Parse fields to be updated
    auto updates = parse_update_fields(argv[0], {argv + 5, argv + argc});

    // I/O context
    boost::asio::io_context ctx;

    // Connection
    boost::mysql::any_connection conn(ctx);

    // Connection configuration
    boost::mysql::connect_params params;
    params.server_address.emplace_host_and_port(argv[3]);
    params.username = argv[1];
    params.password = argv[2];
    params.database = "boost_mysql_examples";
    params.ssl = boost::mysql::ssl_mode::disable;

    // Connect to the server. This will take care of resolving the provided
    // hostname to an IP address, connect to that address, and establish
    // the MySQL session.
    conn.connect(params);

    // Compose
    std::string query = boost::mysql::format_sql(
        "UPDATE employee SET {} WHERE id = {}",
        conn.format_opts().value(),
        updates,
        employee_id
    );

    // Execute
    boost::mysql::results result;
    conn.execute(query, result);

    // Get the updated employee
    query = boost::mysql::format_sql(
        "SELECT first_name, last_name, salary, company_id FROM employee WHERE id = {}",
        conn.format_opts().value(),
        employee_id
    );
    conn.execute(query, result);

    if (result.rows().empty())
    {
        std::cerr << "employee_id=" << employee_id << " not found" << std::endl;
        exit(1);
    }

    // Print it
    const auto employee = result.rows().at(0);
    std::cout << "Updated employee with id=" << employee_id << ":\n"
              << "  first_name: " << employee.at(0) << "\n  last_name: " << employee.at(1)
              << "\n  salary: " << employee.at(2) << "\n  company_id: " << employee.at(3) << std::endl;

    // Notify the MySQL server we want to quit, then close the underlying connection.
    conn.close();
}

int main(int argc, char** argv)
{
    try
    {
        main_impl(argc, argv);
    }
    catch (const boost::mysql::error_with_diagnostics& err)
    {
        // You will only get this type of exceptions if you use throw_on_error.
        // Some errors include additional diagnostics, like server-provided error messages.
        // Security note: diagnostics::server_message may contain user-supplied values (e.g. the
        // field value that caused the error) and is encoded using to the connection's encoding
        // (UTF-8 by default). Treat is as untrusted input.
        std::cerr << "Error: " << err.what() << '\n'
                  << "Server diagnostics: " << err.get_diagnostics().server_message() << std::endl;
        return 1;
    }
    catch (const std::exception& err)
    {
        std::cerr << "Error: " << err.what() << std::endl;
        return 1;
    }
}

//]
