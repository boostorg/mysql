//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/describe/class.hpp>

#ifdef BOOST_DESCRIBE_CXX14

//[example_batch_inserts

// Uses client-side SQL formatting to implement a dynamic filter.
// If you're implementing a filter with many options that can be
// conditionally enabled, this pattern may be useful for you.
//
// Client-side SQL formatting is an experimental feature.

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/character_set.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>
#include <boost/mysql/format_sql.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/core/span.hpp>
#include <boost/describe/class.hpp>
#include <boost/describe/members.hpp>
#include <boost/describe/modifiers.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/value_to.hpp>

#include <fstream>
#include <iostream>
#include <string>

using boost::mysql::error_code;
using boost::mysql::string_view;
namespace describe = boost::describe;

// TODO: C++14

struct employee
{
    std::string first_name;
    std::string last_name;
    std::string company_id;
    double salary;
};
BOOST_DESCRIBE_STRUCT(employee, (), (first_name, last_name, company_id, salary))

[[noreturn]] static void usage(const char* prog_name)
{
    std::cerr << "Usage: " << prog_name << " <username> <password> <server-hostname> <input-file>\n";
    exit(1);
}

// Reads a file into memory
static std::string read_file(const char* file_name)
{
    std::ifstream ifs(file_name);
    if (!ifs)
        throw std::runtime_error("Cannot open file: " + std::string(file_name));
    return std::string(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
}

static std::string compose_batch_insert(
    boost::mysql::format_options opts,
    const std::vector<employee>& employees
)
{
    boost::mysql::format_context ctx(opts);
    ctx.append_raw("INSERT INTO employee (first_name, last_name, company_id, salary) VALUES ");
    bool is_first = true;
    for (const auto& emp : employees)
    {
        if (!is_first)
            ctx.append_raw(", ");
        ctx.append_raw("(")
            .append_value(emp.first_name)
            .append_raw(", ")
            .append_value(emp.last_name)
            .append_raw(", ")
            .append_value(emp.company_id)
            .append_raw(", ")
            .append_value(emp.salary)
            .append_raw(")");
        is_first = false;
    }
    return ctx.get().value();
}

void main_impl(int argc, char** argv)
{
    if (argc != 5)
    {
        usage(argv[0]);
        exit(1);
    }

    // Read input file
    auto contents = read_file(argv[4]);

    // Parse it as JSON
    auto values = boost::json::value_to<std::vector<employee>>(boost::json::parse(contents));

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

    // Connect to the server. This will take care of resolving the provided
    // hostname to an IP address, connect to that address, and establish
    // the MySQL session.
    conn.connect(params);

    // Compose the query
    std::string query = compose_batch_insert(conn.format_opts().value(), values);

    // Execute
    boost::mysql::results result;
    conn.execute(query, result);
    std::cout << "Done\n";

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

#else

int main()
{
    std::cout << "Sorry, your compiler doesn't have the required capabilities to run this example"
              << std::endl;
}

#endif
