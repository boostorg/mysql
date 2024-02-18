//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/client_errc.hpp>

#include <boost/describe/class.hpp>

#ifdef BOOST_DESCRIBE_CXX14

//[example_batch_inserts_generic

// Uses client-side SQL formatting to implement batch inserts
// for any type T with Boost.Describe metadata. It shows how to
// extend format_sql by specializing formatter.
//
// The program reads a JSON file containing a list of employees
// and inserts it into the employee table.
//
// This example requires C++14 to work.
//
// Note: client-side SQL formatting is an experimental feature.

#include <boost/mysql/any_connection.hpp>
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
namespace mp11 = boost::mp11;

/**
 * An example Boost.Describe struct. Our code will work with any struct like this,
 * as long as it has metadata as provided by BOOST_DESCRIBE_STRUCT.
 * We will use this type as an example.
 */
struct employee
{
    std::string first_name;
    std::string last_name;
    std::string company_id;
    std::int64_t salary;  // in dollars per year
};
BOOST_DESCRIBE_STRUCT(employee, (), (first_name, last_name, company_id, salary))

/**
 * Represents a list of objects to be formatted as a list in an INSERT statement.
 * T must be a struct annotated with Boost.Describe metadata.
 * The idea is to make the following work:
 *
 *   std::vector<employee> employees = ... ;
 *   format_sql("INSERT INTO t VALUES {}", opts, insert_list<employee>(employees));
 */
template <class T>
struct insert_list
{
    boost::span<const T> values;
};

/**
 * Represents field names for a Boost.Describe struct T.
 * The idea is to make the following work:
 *
 *   format_sql("INSERT INTO t ({}) VALUES ...", opts, field_name_list<employee>());
 *
 * Generating something like: "INSERT INTO t (`first_name`, `last_name`, `company_id`, `salary`) VALUES ..."
 */
template <class T>
struct field_name_list
{
};

// Helper to reflect a Boost.Descibe type T.
// This retrieves all public data members, including inherited ones.
template <class T>
using public_members = describe::describe_members<T, describe::mod_public | describe::mod_inherited>;

// To make a type U formattable, we need to specialize boost::mysql::formatter<U>
namespace boost {
namespace mysql {

// Make insert_list<T> formattable
template <class T>
struct formatter<insert_list<T>>
{
    // Helper function. This is not required by Boost.MySQL.
    // Adds a single value of type T into the format_context.
    // For an employee, it might generate something like:
    //   "('John', 'Doe', 'HGS', 35000)"
    static void format_single(const T& value, format_context_base& ctx)
    {
        // Opening bracket
        ctx.append_raw("(");

        // We must build a comma-separated list. The first member is not preceeded by a comma.
        bool is_first = true;

        // Iterate over all members of T
        mp11::mp_for_each<public_members<T>>([&](auto D) {
            // Comma separator
            if (!is_first)
            {
                ctx.append_raw(", ");
            }
            is_first = false;

            // Insert the actual member. value.*D.pointer will get
            // each of the data members of our struct.
            // append_value will format the supplied value according to its type,
            // as if it was a {} replacement field: strings are escaped and quoted,
            // doubles are formatted as number literals, and so on.
            ctx.append_value(value.*D.pointer);
        });

        // Closing bracket
        ctx.append_raw(")");
    }

    // Boost.MySQL requires us to define this function. It should take
    // our value as first argument, and a format_context_base& as the second.
    // It should format the value into the context.
    // format_context_base has append_raw and append_value, like format_context.
    static void format(const insert_list<T>& values, format_context_base& ctx)
    {
        // We need one record, at least. If this is not the case, we can use
        // add_error to report the error and exit. This will cause format_sql to throw.
        if (values.values.empty())
        {
            ctx.add_error(client_errc::unformattable_value);
            return;
        }

        // Build a comma-separated list
        bool is_first = true;
        for (const T& val : values.values)
        {
            // Comma separator
            if (!is_first)
            {
                ctx.append_raw(", ");
            }
            is_first = false;

            // Values
            format_single(val, ctx);
        }
    }
};

// Make field_name_list<T> formattable
template <class T>
struct formatter<field_name_list<T>>
{
    // Recall that given a type like employee, we want to output an identifier list:
    //    "`first_name`, `last_name`, `company_id`, `salary`"
    static void format(const field_name_list<T>&, format_context_base& ctx)
    {
        // We must build a comma-separated list. The first member is not preceeded by a comma.
        bool is_first = true;

        // Iterate over all members of T
        mp11::mp_for_each<public_members<T>>([&](auto D) {
            // Comma separator
            if (!is_first)
            {
                ctx.append_raw(", ");
            }
            is_first = false;

            // Output the field's name.
            // D.name is a const char* containing the T's field names.
            // identifier wraps a string to be formatted as a SQL identifier
            // (i.e. `first_name`, rather than 'first_name').
            ctx.append_value(boost::mysql::identifier(D.name));
        });
    }
};

}  // namespace mysql
}  // namespace boost

// Reads a file into memory
std::string read_file(const char* file_name)
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

    // Connect to the server
    conn.connect(params);

    // Compose the query. We've managed to make all out types formattable,
    // so we can use format_sql.
    // Recall that format_opts() returns a system::result<format_options>,
    // which can contain an error if the connection doesn't know which character set is using.
    // Use set_character_set if this happens.
    std::string query = boost::mysql::format_sql(
        conn.format_opts().value(),
        "INSERT INTO employee ({}) VALUES {}",
        field_name_list<employee>(),
        insert_list<employee>{values}
    );

    // Execute the query as usual.
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
