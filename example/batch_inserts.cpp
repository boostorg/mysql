//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/describe/class.hpp>

#ifdef BOOST_DESCRIBE_CXX14

//[example_batch_inserts

// Uses client-side SQL formatting to implement batch inserts
// for a specific type.
// The program reads a JSON file containing a list of employees
// and inserts it into the employee table.
//
// This example requires C++14 to work because it uses Boost.Describe
// to simplify JSON parsing. All Boost.MySQL features used are C++11 compatible.

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/sequence.hpp>
#include <boost/mysql/with_params.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/describe/members.hpp>
#include <boost/describe/modifiers.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/value_to.hpp>

#include <fstream>
#include <iostream>
#include <string>

/**
 * We will use Boost.Describe to easily parse the JSON file
 * into a std::vector<employee>. The JSON file contain an array
 * of objects like the following:
 * {
 *     "first_name": "Some string",
 *     "last_name": "Some other string",
 *     "company_id": "String",
 *     "salary": 20000
 * }
 */
struct employee
{
    std::string first_name;
    std::string last_name;
    std::string company_id;
    std::int64_t salary;  // in dollars per year
};

// Adds reflection capabilities to employee. Required by the JSON parser.
BOOST_DESCRIBE_STRUCT(employee, (), (first_name, last_name, company_id, salary))

// Reads a file into memory
static std::string read_file(const char* file_name)
{
    std::ifstream ifs(file_name);
    if (!ifs)
        throw std::runtime_error("Cannot open file: " + std::string(file_name));
    return std::string(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
}

void main_impl(int argc, char** argv)
{
    if (argc != 5)
    {
        std::cerr << "Usage: " << argv[0] << " <username> <password> <server-hostname> <input-file>\n";
        exit(1);
    }

    // Read our JSON file into memory
    auto contents = read_file(argv[4]);

    // Parse the JSON. json::parse parses the string into a DOM,
    // and json::value_to validates the JSON schema, parsing values into employee structures
    auto values = boost::json::value_to<std::vector<employee>>(boost::json::parse(contents));

    // We need one employee, at least
    if (values.empty())
    {
        std::cerr << "Input file should contain one employee, at least\n";
        exit(1);
    }

    // Create an I/O context, required by all I/O objects
    boost::asio::io_context ctx;

    // Create a connection. Note that client-side SQL formatting
    // requires us to use the newer any_connection.
    boost::mysql::any_connection conn(ctx);

    // Connection configuration. By default, connections use the utf8mb4 character set
    // (MySQL's name for regular UTF-8).
    boost::mysql::connect_params params;
    params.server_address.emplace_host_and_port(argv[3]);
    params.username = argv[1];
    params.password = argv[2];
    params.database = "boost_mysql_examples";

    // A results object to hold the result of executing our SQL query
    boost::mysql::results result;

    // Connect to the server
    conn.connect(params);

    // A function describing how to format a single employee object. Used with mysql::sequence.
    auto format_employee_fn = [](const employee& emp, boost::mysql::format_context_base& ctx) {
        // format_context_base can be used to build query strings incrementally.
        // Used internally by the sequence() formatter.
        // format_sql_to expands a format string, replacing {} fields,
        // and appends the result to the passed context.
        // When formatted, strings are quoted and escaped as string literals.
        // ints are formatted as number literals.
        boost::mysql::format_sql_to(
            ctx,
            "({}, {}, {}, {})",
            emp.first_name,
            emp.last_name,
            emp.company_id,
            emp.salary
        );
    };

    // Compose and execute the batch INSERT. When passed to execute(), with_params
    // replaces placeholders ({}) by actual parameter values before sending the query to the server.
    // When inserting two employees, something like the following may be generated:
    // INSERT INTO employee (first_name, last_name, company_id, salary)
    //     VALUES ('John', 'Doe', 'HGS', 20000), ('Rick', 'Smith', 'LLC', 50000)
    // By default, sequence copies the input range, but we don't need this here,
    // so we disable the copy by calling ref()
    conn.execute(
        boost::mysql::with_params(
            "INSERT INTO employee (first_name, last_name, company_id, salary) VALUES {}",
            boost::mysql::sequence(std::ref(values), format_employee_fn)
        ),
        result
    );
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
        // Some errors include additional diagnostics, like server-provided error messages.
        // Security note: diagnostics::server_message may contain user-supplied values (e.g. the
        // field value that caused the error) and is encoded using to the connection's character set
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

#include <iostream>

int main()
{
    std::cout << "Sorry, your compiler doesn't have the required capabilities to run this example"
              << std::endl;
}

#endif
