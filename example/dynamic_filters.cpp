//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//[example_dynamic_filter

// Uses client-side SQL formatting to implement a dynamic filter.
// If you're implementing a filter with many options that can be
// conditionally enabled, this pattern may be useful for you.
//
// Client-side SQL formatting is an experimental feature.

#include <boost/mysql/any_address.hpp>
#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>
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

#include <iomanip>
#include <iostream>
#include <string>

using boost::mysql::error_code;
using boost::mysql::string_view;

void print_employee(boost::mysql::row_view employee)
{
    std::cout << "id: " << employee.at(0) << ", first_name: " << std::setw(16) << employee.at(1)
              << ", last_name: " << std::setw(16) << employee.at(2) << ", company: " << employee.at(3)
              << ", salary: " << employee.at(4) << '\n';
}

struct filters
{
    boost::optional<string_view> company_id;
    boost::optional<string_view> first_name;
    boost::optional<string_view> last_name;
    boost::optional<double> min_salary;
    boost::optional<string_view> order_by;
};

[[noreturn]] static void usage(const char* prog_name)
{
    std::cerr << "Usage: " << prog_name << " <username> <password> <server-hostname> [filters]\n";
    exit(1);
}

static filters parse_filters(const char* prog_name, boost::span<char* const> argv)
{
    // Available options
    const string_view company_id_prefix = "--company-id=";
    const string_view first_name_prefix = "--first-name=";
    const string_view last_name_prefix = "--last-name=";
    const string_view min_salary_prefix = "--min-salary=";
    const string_view order_by_prefix = "--order-by=";

    // Parse the command line
    filters res;
    for (string_view arg : argv)
    {
        if (arg.starts_with(company_id_prefix))
        {
            res.company_id = arg.substr(company_id_prefix.size());
        }
        else if (arg.starts_with(first_name_prefix))
        {
            res.first_name = arg.substr(first_name_prefix.size());
        }
        else if (arg.starts_with(last_name_prefix))
        {
            res.last_name = arg.substr(last_name_prefix.size());
        }
        else if (arg.starts_with(min_salary_prefix))
        {
            res.min_salary = std::stod(arg.substr(min_salary_prefix.size()));
        }
        else if (arg.starts_with(order_by_prefix))
        {
            auto field_name = arg.substr(order_by_prefix.size());
            if (field_name != "id" && field_name != "first_name" && field_name != "last_name" &&
                field_name != "salary")
            {
                std::cerr << "Order-by: invalid field " << field_name << std::endl;
                usage(prog_name);
            }
            res.order_by = field_name;
        }
        else
        {
            std::cerr << "Unrecognized option: " << arg << std::endl;
            usage(prog_name);
        }
    }

    // We should have at least one filter
    if (!res.company_id && !res.first_name && !res.last_name && !res.min_salary)
    {
        std::cerr << "At least one filter should be specified" << std::endl;
        usage(prog_name);
    }

    return res;
}

std::string compose_get_employees_query(const boost::mysql::any_connection& conn, const filters& filts)
{
    boost::mysql::format_context ctx(conn.format_opts().value());

    bool has_prev = false;

    ctx.append_raw("SELECT id, first_name, last_name, company_id, salary FROM employee WHERE ");

    if (filts.first_name)
    {
        ctx.append_raw("first_name = ");
        ctx.append_value(*filts.first_name);
        has_prev = true;
    }

    if (filts.last_name)
    {
        if (has_prev)
            ctx.append_raw(" AND ");
        ctx.append_raw("last_name = ");
        ctx.append_value(*filts.last_name);
        has_prev = true;
    }

    if (filts.company_id)
    {
        if (has_prev)
            ctx.append_raw(" AND ");
        ctx.append_raw("company_id = ");
        ctx.append_value(*filts.company_id);
        has_prev = true;
    }

    if (filts.min_salary)
    {
        if (has_prev)
            ctx.append_raw(" AND ");
        ctx.append_raw("salary >= ");
        ctx.append_value(*filts.min_salary);
    }

    if (filts.order_by)
    {
        ctx.append_raw(" ORDER BY ");
        ctx.append_value(boost::mysql::identifier(*filts.order_by));
    }

    return ctx.get().value();
}

void main_impl(int argc, char** argv)
{
    if (argc <= 4)
    {
        std::cerr << "Usage: " << argv[0] << " <username> <password> <server-hostname> [filters]\n";
        exit(1);
    }

    // Parse filters
    filters filts = parse_filters(argv[0], {argv + 4, argv + argc});

    // I/O context
    boost::asio::io_context ctx;

    /**
     * The entry point. We spawn a stackful coroutine using boost::asio::spawn.
     *
     * The coroutine will actually start running when we call io_context::run().
     * It will suspend every time we call one of the asynchronous functions, saving
     * all information it needs for resuming. When the asynchronous operation completes,
     * the coroutine will resume in the point it was left.
     */
    boost::asio::spawn(
        ctx.get_executor(),
        [argv, &filts](boost::asio::yield_context yield) {
            // Connection
            boost::mysql::any_connection conn(yield.get_executor());

            // Connection configuration
            boost::mysql::connect_params params;
            params.server_address.emplace_host_and_port(argv[3]);
            params.username = argv[1];
            params.password = argv[2];
            params.database = "boost_mysql_examples";

            // This error_code and diagnostics will be filled if an
            // operation fails. We will check them for every operation we perform.
            boost::mysql::error_code ec;
            boost::mysql::diagnostics diag;

            // Connect to the server. This will take care of resolving the provided
            // hostname to an IP address, connect to that address, and establish
            // the MySQL session.
            conn.async_connect(params, diag, yield[ec]);
            boost::mysql::throw_on_error(ec, diag);

            // Compose
            std::string query = compose_get_employees_query(conn, filts);

            // Execute
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

    // Don't forget to call run()! Otherwise, your program
    // will not spawn the coroutine and will do nothing.
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
