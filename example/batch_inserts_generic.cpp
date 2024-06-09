//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/constant_string_view.hpp>
#include <boost/mysql/field_view.hpp>

#include <boost/describe/class.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/tuple.hpp>

#include <array>
#include <cstddef>

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

// Helper to reflect a Boost.Descibe type T.
// This retrieves all public data members, including inherited ones.
template <class T>
using public_members = describe::describe_members<T, describe::mod_public | describe::mod_inherited>;

// The number of public members of a struct
template <class T>
constexpr std::size_t num_public_members = mp11::mp_size<public_members<T>>::value;

// Gets the member names of a struct, as an array of string views
// For employee, generates {"first_name", "last_name", "company_id", "salary"}
template <class T>
constexpr std::array<string_view, num_public_members<T>> get_field_names()
{
    return mp11::tuple_apply(
        [](auto... descriptors) {
            return std::array<string_view, num_public_members<T>>{descriptors.name...};
        },
        mp11::mp_rename<public_members<T>, std::tuple>()
    );
}

struct describe_struct_format_fn
{
    template <class T>
    void operator()(const T& value, boost::mysql::format_context_base& ctx) const
    {
        // Convert the struct into an array of type-erased format args
        // TODO: this doesn't work for optionals or things with custom formatters
        auto args = mp11::tuple_apply(
            [&value](auto... descriptors) {
                return std::array<boost::mysql::field_view, num_public_members<T>>{
                    boost::mysql::field_view(value.*descriptors.pointer)...
                };
            },
            mp11::mp_rename<public_members<T>, std::tuple>()
        );

        // Join them
        boost::mysql::format_sql_to(ctx, "({})", boost::mysql::join(args));
    }
};

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

    // We need one value to insert, at least
    if (values.empty())
    {
        std::cerr << argv[0] << ": the JSON file should contain at least one employee\n";
        exit(1);
    }

    // Compose the query. We've managed to make all out types formattable,
    // so we can use format_sql.
    // Recall that format_opts() returns a system::result<format_options>,
    // which can contain an error if the connection doesn't know which character set is using.
    // Use set_character_set if this happens.
    std::string query = boost::mysql::format_sql(
        conn.format_opts().value(),
        "INSERT INTO employee ({}) VALUES {}",
        // Field names
        boost::mysql::join(
            get_field_names<employee>(),
            [](string_view field_name, boost::mysql::format_context_base& ctx) {
                boost::mysql::format_sql_to(ctx, "{}", boost::mysql::identifier(field_name));
            }
        ),

        // Field values
        boost::mysql::join(values, describe_struct_format_fn())
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
