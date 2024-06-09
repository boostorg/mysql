//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//[example_dynamic_filters

// Uses client-side SQL formatting to implement a dynamic filter.
// If you're implementing a filter with many options that can be
// conditionally enabled, this pattern may be useful for you.
//
// Client-side SQL formatting is an experimental feature.

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/character_set.hpp>
#include <boost/mysql/constant_string_view.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/format_sql.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/row_view.hpp>
#include <boost/mysql/string_view.hpp>
#include <boost/mysql/throw_on_error.hpp>

#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/core/span.hpp>
#include <boost/optional/optional.hpp>

#include <cassert>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using boost::mysql::error_code;
using boost::mysql::field_view;
using boost::mysql::string_view;

// Prints an employee row to stdout
void print_employee(boost::mysql::row_view employee)
{
    std::cout << "id: " << employee.at(0)                             // field 0: id
              << ", first_name: " << std::setw(16) << employee.at(1)  // field 1: first_name
              << ", last_name: " << std::setw(16) << employee.at(2)   // field 2: last_name
              << ", company_id: " << employee.at(3)                   // field 3: company_id
              << ", salary: " << employee.at(4) << '\n';              // field 4: salary
}

enum class op_type
{
    lt,
    lte,
    eq,
    gt,
    gte,
};

string_view op_type_to_string(op_type value)
{
    switch (value)
    {
    case op_type::lt: return "<";
    case op_type::lte: return "<=";
    case op_type::eq: return "=";
    case op_type::gte: return ">=";
    case op_type::gt: return ">";
    default: assert(false); return "";
    }
}

// An individual filter to apply
struct filter_item
{
    string_view field_name;
    op_type op;
    field_view field_value;
};

// Command line arguments
struct cmdline_args
{
    // MySQL username to use during authentication.
    string_view username;

    // MySQL password to use during authentication.
    string_view password;

    // Hostname where the MySQL server is listening.
    string_view server_hostname;

    // The filters to apply
    std::vector<filter_item> filts;

    // If order_by.has_value(), order employees using the given field
    boost::optional<string_view> order_by;
};

// Parses the command line
static cmdline_args parse_cmdline_args(int argc, char** argv)
{
    // Available options
    const string_view company_id_prefix = "--company-id=";
    const string_view first_name_prefix = "--first-name=";
    const string_view last_name_prefix = "--last-name=";
    const string_view min_salary_prefix = "--min-salary=";
    const string_view order_by_prefix = "--order-by=";

    // Helper function to print the usage message and exit
    auto print_usage_and_exit = [argv]() {
        std::cerr << "Usage: " << argv[0] << " <username> <password> <server-hostname> [filters]\n";
        exit(1);
    };

    // Check number of arguments
    if (argc <= 4)
        print_usage_and_exit();

    // Parse the required arguments
    cmdline_args res;
    res.username = argv[1];
    res.password = argv[2];
    res.server_hostname = argv[3];

    // Parse the filters
    for (int i = 4; i < argc; ++i)
    {
        string_view arg = argv[i];

        // Attempt to match the argument against each prefix
        if (arg.starts_with(company_id_prefix))
        {
            auto value = arg.substr(company_id_prefix.size());
            res.filts.push_back({"company_id", op_type::eq, field_view(value)});
        }
        else if (arg.starts_with(first_name_prefix))
        {
            auto value = arg.substr(first_name_prefix.size());
            res.filts.push_back({"first_name", op_type::eq, field_view(value)});
        }
        else if (arg.starts_with(last_name_prefix))
        {
            auto value = arg.substr(last_name_prefix.size());
            res.filts.push_back({"last_name", op_type::eq, field_view(value)});
        }
        else if (arg.starts_with(min_salary_prefix))
        {
            auto value = std::stod(arg.substr(min_salary_prefix.size()));
            res.filts.push_back({"salary", op_type::gte, field_view(value)});
        }
        else if (arg.starts_with(order_by_prefix))
        {
            auto field_name = arg.substr(order_by_prefix.size());

            // For security, validate the passed field against a set of whitelisted fields
            if (field_name != "id" && field_name != "first_name" && field_name != "last_name" &&
                field_name != "salary")
            {
                std::cerr << "Order-by: invalid field " << field_name << std::endl;
                print_usage_and_exit();
            }
            res.order_by = field_name;
        }
        else
        {
            std::cerr << "Unrecognized option: " << arg << std::endl;
            print_usage_and_exit();
        }
    }

    // We should have at least one filter
    if (res.filts.empty())
    {
        std::cerr << "At least one filter should be specified" << std::endl;
        print_usage_and_exit();
    }

    return res;
}

// Composes a SELECT query to retrieve employees according to the passed filters.
//
std::string compose_get_employees_query(
    boost::mysql::format_options opts,
    const std::vector<filter_item>& filts,
    boost::optional<string_view> order_by
)
{
    // A format context allows composing queries incrementally
    boost::mysql::format_context ctx(opts);

    // Add the query with the filters
    boost::mysql::format_sql_to(
        ctx,
        "SELECT id, first_name, last_name, company_id, salary FROM employee WHERE {}",
        boost::mysql::sequence(
            filts,
            [](filter_item item, boost::mysql::format_context_base& ctx) {
                ctx.append_value(boost::mysql::identifier(item.field_name))
                    .append_raw(boost::mysql::runtime(op_type_to_string(item.op)))
                    .append_value(item.field_value);
            },
            " AND "
        )
    );

    // Add the order by
    if (order_by)
    {
        // identifier formats a string as a SQL identifier, instead of a string literal.
        // For instance, this may generate "ORDER BY `first_name`"
        boost::mysql::format_sql_to(ctx, " ORDER BY {}", boost::mysql::identifier(*order_by));
    }

    // Get our generated query
    return std::move(ctx).get().value();
}

void main_impl(int argc, char** argv)
{
    // Parse the command line
    cmdline_args args = parse_cmdline_args(argc, argv);

    // Create an I/O context, required by all I/O objects
    boost::asio::io_context ctx;

    /**
     * Spawn a stackful coroutine using boost::asio::spawn.
     * The coroutine suspends every time we call an asynchronous function, and
     * will resume when it completes.
     * Note that client-side SQL formatting can be used with both sync and async functions.
     */
    boost::asio::spawn(
        ctx.get_executor(),
        [args](boost::asio::yield_context yield) {
            // Create a connection. Note that client-side SQL formatting
            // requires us to use the newer any_connection.
            boost::mysql::any_connection conn(yield.get_executor());

            // Connection configuration. By default, connections use the utf8mb4 character set
            // (MySQL's name for regular UTF-8).
            boost::mysql::connect_params params;
            params.server_address.emplace_host_and_port(args.server_hostname);
            params.username = args.username;
            params.password = args.password;
            params.database = "boost_mysql_examples";

            // This error_code and diagnostics will be filled if an
            // operation fails. We will check them for every operation we perform.
            boost::mysql::error_code ec;
            boost::mysql::diagnostics diag;

            // Connect to the server
            conn.async_connect(params, diag, yield[ec]);
            boost::mysql::throw_on_error(ec, diag);

            // Compose the query. format_opts() returns a system::result<format_options>,
            // containing the options required by format_context. format_opts() may return
            // an error if the connection doesn't know which character set is using -
            // use async_set_character_set if this happens.
            std::string query = compose_get_employees_query(
                conn.format_opts().value(),
                args.filts,
                args.order_by
            );

            // Execute the query as usual. Note that, unlike with prepared statements,
            // formatting happened in the client, and not in the server.
            boost::mysql::results result;
            conn.async_execute(query, result, diag, yield[ec]);
            boost::mysql::throw_on_error(ec, diag);

            // Print the employees
            for (boost::mysql::row_view employee : result.rows())
            {
                print_employee(employee);
            }

            // Notify the MySQL server we want to quit, then close the underlying connection.
            conn.async_close(diag, yield[ec]);
            boost::mysql::throw_on_error(ec, diag);
        },
        // If any exception is thrown in the coroutine body, rethrow it.
        [](std::exception_ptr ptr) {
            if (ptr)
            {
                std::rethrow_exception(ptr);
            }
        }
    );

    // Don't forget to call run()! Otherwise, your program will do nothing.
    ctx.run();
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
