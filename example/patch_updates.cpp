//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//[example_patch_updates

// Uses client-side SQL formatting to implement dynamic updates
// with PATCH-like semantics.
// The program updates an employee by ID, modifying fields
// as provided by command-line arguments, and leaving all other
// fields unmodified.
//
// This example specializes formatter to make custom types
// compatible with format_sql.
//
// Note: client-side SQL formatting is an experimental feature.

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

/**
 * Represents a single update as a name, value pair.
 * The idea is to use command-line arguments to compose
 * a std::vector<update_field> with the fields to be updated,
 * and make the following work:
 *
 *    std::vector<update_field> updates {
 *       { "fist_name", field_view("John") },
 *       { "salary", field_view(35000)     },
 *    };
 *    format_sql("UPDATE employee SET {} WHERE id = {}", opts, updates, 42);
 *
 * "UPDATE employee SET `first_name` = 'John', `salary` = 35000 WHERE id = 42"
 */
struct update_field
{
    // The field name to set (i.e. the column name)
    string_view field_name;

    // The value to set the field to. Recall that field_view is
    // a variant-like type that can hold all types that MySQL supports.
    field_view field_value;
};

namespace boost {
namespace mysql {

// Specialize formatter so that format_sql accepts a std::vector<update_field>
template <>
struct formatter<std::vector<update_field>>
{
    // Boost.MySQL requires us to define this function. It should take
    // our value as first argument, and a format_context_base& as the second.
    // It should format the value into the context.
    // format_context_base has append_raw and append_value, like format_context.
    static void format(const std::vector<update_field>& value, format_context_base& ctx)
    {
        // Precondition: we must never pass empty update lists
        assert(!value.empty());

        // Build a comma-separated list
        bool is_first = true;
        for (const auto& update : value)
        {
            // Comma separator
            if (!is_first)
            {
                ctx.append_raw(", ");
            }
            is_first = false;

            // Output the field's name, an equal sign, and the field's value.
            // identifier wraps a string to be formatted as a SQL identifier
            // (i.e. `first_name`, rather than 'first_name').
            ctx.append_value(boost::mysql::identifier(update.field_name))
                .append_raw(" = ")
                .append_value(update.field_value);
        }
    }
};

}  // namespace mysql
}  // namespace boost

// Contains the parsed command-line arguments
struct cmdline_args
{
    // MySQL username to use during authentication.
    string_view username;

    // MySQL password to use during authentication.
    string_view password;

    // Hostname where the MySQL server is listening.
    string_view server_hostname;

    // The ID of the employee we want to update.
    std::int64_t employee_id{};

    // A list of name, value pairs containing the employee fields to update.
    std::vector<update_field> updates;
};

// Parses the command line arguments, calling exit on failure.
static cmdline_args parse_cmdline_args(int argc, char** argv)
{
    // Available options
    const string_view company_id_prefix = "--company-id=";
    const string_view first_name_prefix = "--first-name=";
    const string_view last_name_prefix = "--last-name=";
    const string_view salary_prefix = "--salary=";

    // Helper function to print the usage message and exit
    auto print_usage_and_exit = [argv]() {
        std::cerr << "Usage: " << argv[0]
                  << " <username> <password> <server-hostname> employee_id [updates]\n";
        exit(1);
    };

    // Check number of arguments
    if (argc <= 5)
        print_usage_and_exit();

    // Parse the required arguments
    cmdline_args res;
    res.username = argv[1];
    res.password = argv[2];
    res.server_hostname = argv[3];
    res.employee_id = std::stoll(argv[4]);

    // Parse the requested updates
    for (int i = 5; i < argc; ++i)
    {
        // Get the argument
        string_view arg = argv[i];

        // Attempt to match it with the options we have
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
            print_usage_and_exit();
        }
    }

    return res;
}

void main_impl(int argc, char** argv)
{
    // Parse the command line
    cmdline_args args = parse_cmdline_args(argc, argv);

    // Create an I/O context, required by all I/O objects
    boost::asio::io_context ctx;

    // Create a connection. Note that client-side SQL formatting
    // requires us to use the newer any_connection.
    boost::mysql::any_connection conn(ctx);

    // Connection configuration. By default, connections use the utf8mb4 character set
    // (MySQL's name for regular UTF-8).
    boost::mysql::connect_params params;
    params.server_address.emplace_host_and_port(args.server_hostname);
    params.username = args.username;
    params.password = args.password;
    params.database = "boost_mysql_examples";

    // Connect to the server
    conn.connect(params);

    // Compose the query. We've managed to make all out types formattable,
    // so we can use format_sql.
    // Recall that format_opts() returns a system::result<format_options>,
    // which can contain an error if the connection doesn't know which character set is using.
    // Use set_character_set if this happens.
    std::string query = boost::mysql::format_sql(
        "UPDATE employee SET {} WHERE id = {}",
        conn.format_opts().value(),
        args.updates,
        args.employee_id
    );

    // Execute the query as usual.
    boost::mysql::results result;
    conn.execute(query, result);

    // Get the updated employee
    query = boost::mysql::format_sql(
        "SELECT first_name, last_name, salary, company_id FROM employee WHERE id = {}",
        conn.format_opts().value(),
        args.employee_id
    );
    conn.execute(query, result);

    if (result.rows().empty())
    {
        std::cerr << "employee_id=" << args.employee_id << " not found" << std::endl;
        exit(1);
    }

    // Print it
    const auto employee = result.rows().at(0);
    std::cout << "Updated employee with id=" << args.employee_id << ":\n"
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
