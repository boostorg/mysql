//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// This file contains all the snippets that are used in the docs.
// They're here so they are built and run, to ensure correctness

#include <boost/mysql/date.hpp>
#include <boost/mysql/datetime.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/field.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/metadata_mode.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/resultset.hpp>
#include <boost/mysql/resultset_view.hpp>
#include <boost/mysql/row.hpp>
#include <boost/mysql/row_view.hpp>
#include <boost/mysql/rows.hpp>
#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/statement.hpp>
#include <boost/mysql/string_view.hpp>
#include <boost/mysql/tcp_ssl.hpp>
#include <boost/mysql/throw_on_error.hpp>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/config.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/system/system_error.hpp>

#include <iostream>
#include <string>
#include <tuple>

#ifndef BOOST_NO_CXX17_HDR_OPTIONAL
#include <optional>
#endif
#ifdef BOOST_ASIO_HAS_CO_AWAIT
#include <boost/asio/experimental/awaitable_operators.hpp>
#endif

using boost::mysql::date;
using boost::mysql::datetime;
using boost::mysql::diagnostics;
using boost::mysql::error_code;
using boost::mysql::error_with_diagnostics;
using boost::mysql::execution_state;
using boost::mysql::field;
using boost::mysql::field_view;
using boost::mysql::metadata_mode;
using boost::mysql::results;
using boost::mysql::row;
using boost::mysql::row_view;
using boost::mysql::rows;
using boost::mysql::rows_view;
using boost::mysql::statement;
using boost::mysql::string_view;
using boost::mysql::tcp_ssl_connection;

#define ASSERT(expr)                                          \
    if (!(expr))                                              \
    {                                                         \
        std::cerr << "Assertion failed: " #expr << std::endl; \
        exit(1);                                              \
    }

const char* get_value_from_user() { return ""; }
std::int64_t get_employee_id() { return 42; }
std::string get_company_id() { return "HGS"; }

//[prepared_statements_execute
// description, price and show_in_store are not trusted, since they may
// have been read from a file or an HTTP endpoint
void insert_product(
    tcp_ssl_connection& conn,
    const statement& stmt,
    string_view description,
    int price,
    bool show_in_store
)
{
    results result;
    conn.execute_statement(
        stmt,
        std::make_tuple(description, price, static_cast<int>(show_in_store)),
        result
    );
}
//]

#ifndef BOOST_NO_CXX17_HDR_OPTIONAL
//[prepared_statements_execute_null
// description, price and show_in_store are not trusted, since they may
// have been read from a file or an HTTP endpoint
void insert_product(
    tcp_ssl_connection& conn,
    const statement& stmt,
    std::optional<string_view> description,
    int price,
    bool show_in_store
)
{
    // if description has a value, description_param will have kind() == field_kind::string
    // and will point to it. Otherwise, description_param.kind() == field_kind::null
    auto description_param = description ? field_view(*description) : field_view();

    // Execute the insert
    results result;
    conn.execute_statement(
        stmt,
        std::make_tuple(description_param, price, static_cast<int>(show_in_store)),
        result
    );
}
//]
#endif

#ifdef BOOST_ASIO_HAS_CO_AWAIT
boost::asio::awaitable<void> overview_coro(tcp_ssl_connection& conn)
{
    //[overview_async_coroutinescpp20
    // Using this CompletionToken, you get C++20 coroutines that communicate
    // errors with error_codes. This way, you can access the diagnostics object.
    constexpr auto token = boost::asio::as_tuple(boost::asio::use_awaitable);

    // Run our query as a coroutine
    diagnostics diag;
    results result;
    auto [ec] = co_await conn.async_query("SELECT 'Hello world!'", result, diag, token);

    // This will throw an error_with_diagnostics in case of failure
    boost::mysql::throw_on_error(ec, diag);
    //]
}

void run_overview_coro(tcp_ssl_connection& conn)
{
    boost::asio::co_spawn(
        conn.get_executor(),
        [&conn] { return overview_coro(conn); },
        [](std::exception_ptr ptr) {
            if (ptr)
            {
                std::rethrow_exception(ptr);
            }
        }
    );
    static_cast<boost::asio::io_context&>(conn.get_executor().context()).run();
}

boost::asio::awaitable<void> dont_run()
{
    using namespace boost::asio::experimental::awaitable_operators;

    // Setup
    boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::tls_client);
    boost::mysql::tcp_ssl_connection conn(co_await boost::asio::this_coro::executor, ssl_ctx);

    //[overview_async_dont
    // Coroutine body
    // DO NOT DO THIS!!!!
    results result1, result2;
    co_await (
        conn.async_query("SELECT 1", result1, boost::asio::use_awaitable) &&
        conn.async_query("SELECT 2", result2, boost::asio::use_awaitable)
    );
    //]
}
#endif

void main_impl(int argc, char** argv)
{
    if (argc != 4)
    {
        std::cerr << "Usage: " << argv[0] << " <username> <password> <server-hostname>\n";
        exit(1);
    }

    //[overview_connection
    // The execution context, required to run I/O operations.
    boost::asio::io_context ctx;

    // The SSL context, required to establish TLS connections.
    // The default SSL options are good enough for us at this point.
    boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::tls_client);

    // Represents a connection to the MySQL server.
    boost::mysql::tcp_ssl_connection conn(ctx.get_executor(), ssl_ctx);
    //]

    //[overview_connect
    // Resolve the hostname to get a collection of endpoints
    boost::asio::ip::tcp::resolver resolver(ctx.get_executor());
    auto endpoints = resolver.resolve(argv[3], boost::mysql::default_port_string);

    // The username and password to use
    boost::mysql::handshake_params params(
        argv[1],                // username
        argv[2],                // password
        "boost_mysql_examples"  // database
    );

    // Connect to the server using the first endpoint returned by the resolver
    conn.connect(*endpoints.begin(), params);
    //]

    {
        //[overview_query_use_case
        results result;
        conn.query("START TRANSACTION", result);
        //]
    }
    {
        //[overview_statement_use_case
        statement stmt = conn.prepare_statement(
            "SELECT first_name FROM employee WHERE company_id = ? AND salary > ?"
        );

        results result;
        conn.execute_statement(stmt, std::make_tuple("HGS", 30000), result);
        //]
    }
    {
        //[overview_views
        // Populate a results object
        results result;
        conn.query("SELECT 'Hello world'", result);

        // results::rows() returns a rows_view. The underlying memory is owned by the results object
        rows_view all_rows = result.rows();

        // Indexing a rows_view yields a row_view. The underlying memory is owned by the results object
        row_view first_row = all_rows.at(0);

        // Indexing a row_view yields a field_view. The underlying memory is owned by the results object
        field_view first_field = first_row.at(0);  // Contains the string "Hello world"

        //]
        ASSERT(first_field.as_string() == "Hello world");

        //[overview_taking_ownership
        // You may use all_rows_owning after result has gone out of scope
        rows all_rows_owning{all_rows};

        // You may use first_row_owning after result has gone out of scope
        row first_row_owning{first_row};

        // You may use first_field_owning after result has gone out of scope
        field first_field_owning{first_field};
        //]
    }
    {
        //[overview_using_fields
        results result;
        conn.query("SELECT 'abc', 42", result);

        // Obtain a field's underlying value using the is_xxx and get_xxx accessors
        field_view f = result.rows().at(0).at(0);  // f points to the string "abc"
        if (f.is_string())
        {
            // we know it's a string, unchecked access
            string_view s = f.get_string();
            std::cout << s << std::endl;  // Use the string as required
        }
        else
        {
            // Oops, something went wrong - schema msimatch?
        }

        // Alternative: use the as_xxx accessor
        f = result.rows().at(0).at(1);
        std::int64_t value = f.as_int64();  // Checked access. Throws if f doesn't contain an int
        std::cout << value << std::endl;    // Use the int as required

        //]
    }
    {
        //[overview_handling_nulls
        results result;

        // Create some test data
        conn.query(
            R"%(
                CREATE TEMPORARY TABLE products (
                    id VARCHAR(50) PRIMARY KEY,
                    description VARCHAR(256)
                )
            )%",
            result
        );
        conn.query("INSERT INTO products VALUES ('PTT', 'Potatoes'), ('CAR', NULL)", result);

        // Retrieve the data. Note that some fields are NULL
        conn.query("SELECT id, description FROM products", result);

        for (row_view r : result.rows())
        {
            field_view description_fv = r.at(1);
            if (description_fv.is_null())
            {
                // Handle the NULL value
                // Note: description_fv.is_string() will return false here; NULL is represented as a separate
                // type
                std::cout << "No description for product_id " << r.at(0) << std::endl;
            }
            else
            {
                // Handle the non-NULL case. Get the underlying value and use it as you want
                // If there is any schema mismatch (and description was not defined as VARCHAR), this will
                // throw
                string_view description = description_fv.as_string();

                // Use description as required
                std::cout << "product_id " << r.at(0) << ": " << description << std::endl;
            }
        }
        //]

        conn.query("DROP TABLE products", result);
    }
    {
        //[overview_statements_setup
        results result;
        conn.query(
            R"%(
                CREATE TEMPORARY TABLE products (
                    id VARCHAR(50) PRIMARY KEY,
                    description VARCHAR(256)
                )
            )%",
            result
        );
        conn.query("INSERT INTO products VALUES ('PTT', 'Potatoes'), ('CAR', 'Carrots')", result);
        //]
    }
    {
        //[overview_statements_prepare
        statement stmt = conn.prepare_statement("SELECT description FROM products WHERE id = ?");
        //]

        //[overview_statements_execute
        // Obtain the product_id from the user. product_id is untrusted input
        const char* product_id = argv[2];

        // Execute the statement
        results result;
        conn.execute_statement(stmt, std::make_tuple(product_id), result);

        // Use result as required
        //]

        conn.query("DROP TABLE products", result);
    }
    {
        //[overview_multifn
        // Create the table and some sample data
        // In a real system, body may be megabaytes long.
        results result;
        conn.query(
            R"%(
                CREATE TEMPORARY TABLE posts (
                    id INT PRIMARY KEY AUTO_INCREMENT,
                    title VARCHAR (256),
                    body TEXT
                )
            )%",
            result
        );
        conn.query(
            R"%(
                INSERT INTO posts (title, body) VALUES
                    ('Post 1', 'A very long post body'),
                    ('Post 2', 'An even longer post body')
            )%",
            result
        );

        // execution_state stores state about our operation, and must be passed to all functions
        execution_state st;

        // Writes the query request and reads the server response, but not the rows
        conn.start_query("SELECT title, body FROM posts", st);

        // Reads all the returned rows, in batches.
        // st.complete() returns true once there are no more rows to read
        while (!st.complete())
        {
            // row_batch will be valid until conn performs the next network operation
            rows_view row_batch = conn.read_some_rows(st);

            for (row_view post : row_batch)
            {
                // Process post as required
                std::cout << "Title:" << post.at(0) << std::endl;
            }
        }
        //]

        conn.query("DROP TABLE posts", result);
    }
    {
        //[overview_errors_sync_errc
        error_code ec;
        diagnostics diag;
        results result;

        // The provided SQL is invalid. The server will return an error.
        // ec will be set to a non-zero value
        conn.query("this is not SQL!", result, ec, diag);

        if (ec)
        {
            // The error code will likely report a syntax error
            std::cout << "Operation failed with error code: " << ec << '\n';

            // diag.server_message() will contain the classic phrase
            // "You have an error in your SQL syntax; check the manual..."
            // Bear in mind that server_message() may contain user input, so treat it with caution
            std::cout << "Server diagnostics: " << diag.server_message() << std::endl;
        }
        //]
    }
    {
        //[overview_errors_sync_exc
        try
        {
            // The provided SQL is invalid. This function will throw an exception.
            results result;
            conn.query("this is not SQL!", result);
        }
        catch (const error_with_diagnostics& err)
        {
            // error_with_diagnostics contains an error_code and a diagnostics object.
            // It inherits from boost::system::system_error.
            std::cout << "Operation failed with error code: " << err.code() << '\n'
                      << "Server diagnostics: " << err.get_diagnostics().server_message() << std::endl;
        }
        //]
    }
#ifdef BOOST_ASIO_HAS_CO_AWAIT
    {
        run_overview_coro(conn);
    }
#endif

    // prepared statements
    {
        //[prepared_statements_prepare
        // Table setup
        results result;
        conn.query(
            R"%(
                CREATE TEMPORARY TABLE products (
                    id INT PRIMARY KEY AUTO_INCREMENT,
                    description VARCHAR(256),
                    price INT NOT NULL,
                    show_in_store TINYINT
                )
            )%",
            result
        );

        // Prepare a statement to insert into this table
        statement stmt = conn.prepare_statement(
            "INSERT INTO products (description, price, show_in_store) VALUES (?, ?, ?)"
        );
        //]

        // Run the two functions, even if this is not shown in discussion
        insert_product(conn, stmt, string_view("This is a product"), 2000, true);
#ifndef BOOST_NO_CXX17_HDR_OPTIONAL
        insert_product(conn, stmt, std::optional<string_view>(), 2000, true);
#endif
        conn.query("DROP TABLE products", result);
    }

    // multi-resultset
    {
        results result;
        conn.query("DROP PROCEDURE IF EXISTS get_employee", result);

        //[multi_resultset_procedure
        conn.query(
            R"(
                CREATE PROCEDURE get_employee(IN pin_employee_id INT)
                BEGIN
                    SELECT * FROM employee WHERE id = pin_employee_id;
                END
            )",
            result
        );
        //]

        //[multi_resultset_call
        // The procedure parameter, employe_id, will likely be obtained from an untrusted source,
        // so we will use a prepared statement
        statement get_employee_stmt = conn.prepare_statement("CALL get_employee(?)");

        // Obtain the parameters required to call the statement, e.g. from a file or HTTP message
        std::int64_t employee_id = get_employee_id();

        // Call the statement
        conn.execute_statement(get_employee_stmt, std::make_tuple(employee_id), result);
        //]

        //[multi_resultset_first_resultset
        rows_view matched_employees = result.at(0).rows();
        // Use matched_employees as required
        //]

        boost::ignore_unused(matched_employees);
    }
    {
        results result;
        conn.query("DROP PROCEDURE IF EXISTS create_employee", result);

        //[multi_resultset_out_params
        // Setup the stored procedure
        conn.query(
            R"(
                CREATE PROCEDURE create_employee(
                    IN  pin_company_id CHAR(10),
                    IN  pin_first_name VARCHAR(100),
                    IN  pin_last_name VARCHAR(100),
                    OUT pout_employee_id INT
                )
                BEGIN
                    START TRANSACTION;
                    INSERT INTO employee (company_id, first_name, last_name)
                        VALUES (pin_company_id, pin_first_name, pin_last_name);
                    SET pout_employee_id = LAST_INSERT_ID();
                    INSERT INTO audit_log (msg) VALUES ('Created new employee...');
                    COMMIT;
                END
            )",
            result
        );

        // To retrieve output parameters, you must use prepared statements. Text queries don't support this
        // We specify placeholders for both IN and OUT parameters
        statement stmt = conn.prepare_statement("CALL create_employee(?, ?, ?, ?)");

        // When executing the statement, we provide an actual value for the IN parameters,
        // and a dummy value for the OUT parameter. This value will be ignored, but it's required by the
        // protocol
        conn.execute_statement(stmt, std::make_tuple("HGS", "John", "Doe", nullptr), result);

        // Retrieve output parameters. This row_view has an element per
        // OUT or INOUT parameter that used a ? placeholder
        row_view output_params = result.out_params();
        std::int64_t new_employee_id = output_params.at(0).as_int64();
        //]

        boost::ignore_unused(new_employee_id);
    }
    {
        boost::mysql::tcp_ssl_connection conn(ctx.get_executor(), ssl_ctx);
        auto endpoint = *resolver.resolve(argv[3], boost::mysql::default_port_string).begin();

        //[multi_resultset_multi_queries
        // The username and password to use
        boost::mysql::handshake_params params(
            argv[1],                // username
            argv[2],                // password
            "boost_mysql_examples"  // database
        );

        // Allows running multiple semicolon-separated in a single call.
        // We must set this before calling connect
        params.set_multi_queries(true);

        // Connect to the server specifying that we want support for multi-queries
        conn.connect(endpoint, params);

        // We can now use the multi-query feature.
        // This will result in three resultsets, one per query.
        results result;
        conn.query(
            R"(
                CREATE TEMPORARY TABLE posts (
                    id INT PRIMARY KEY AUTO_INCREMENT,
                    title VARCHAR (256),
                    body TEXT
                );
                INSERT INTO posts (title, body) VALUES ('Breaking news', 'Something happened!');
                SELECT COUNT(*) FROM posts;
            )",
            result
        );
        //]

        //[multi_resultset_results_as_collection
        // result is actually a random-access collection of resultsets.
        // The INSERT is the 2nd query, so we can access its resultset like this:
        boost::mysql::resultset_view insert_result = result.at(1);

        // A resultset has metadata, rows, and additional data, like the last insert ID:
        std::int64_t post_id = insert_result.last_insert_id();

        // The SELECT result is the third one, so we can access it like this:
        boost::mysql::resultset_view select_result = result.at(2);

        // select_result is a view that points into result.
        // We can take ownership of it using the resultse class:
        boost::mysql::resultset owning_select_result(select_result);  // valid even after result is destroyed

        // We can access rows of resultset objects as usual:
        std::int64_t num_posts = owning_select_result.rows().at(0).at(0).as_int64();
        //]

        boost::ignore_unused(post_id);
        boost::ignore_unused(num_posts);
    }

    // multi-function
    {
        //[multi_function_setup
        results result;
        conn.query(
            R"%(
                CREATE TEMPORARY TABLE posts (
                    id INT PRIMARY KEY AUTO_INCREMENT,
                    title VARCHAR (256),
                    body TEXT
                )
            )%",
            result
        );
        conn.query(
            R"%(
                INSERT INTO posts (title, body) VALUES
                    ('Post 1', 'A very long post body'),
                    ('Post 2', 'An even longer post body')
            )%",
            result
        );

        statement stmt = conn.prepare_statement("SELECT title, body FROM posts");
        //]

        auto read_all_rows = [&](execution_state& st) {
            //[multi_function_read_some_rows
            // st.complete() returns true once the OK packet is received
            while (!st.complete())
            {
                // row_batch will be valid until conn performs the next network operation
                rows_view row_batch = conn.read_some_rows(st);

                for (row_view post : row_batch)
                {
                    // Process post as required
                    std::cout << "Title:" << post.at(0) << std::endl;
                }
            }
            //]
        };

        {
            //[multi_function_start_query
            execution_state st;
            conn.start_query("SELECT title, body FROM posts", st);
            //]

            read_all_rows(st);  // don't compromise further operations
        }

        {
            //[multi_function_start_statement_execution
            execution_state st;
            conn.start_statement_execution(
                stmt,
                std::make_tuple(),  // The statement has no params, so an empty tuple is passed
                st
            );
            //]

            read_all_rows(st);  // don't compromise further operations
            conn.query("DROP TABLE posts", result);
        }

        {
            results result;
            conn.query("DROP PROCEDURE IF EXISTS get_company", result);

            //[multi_function_stored_procedure
            // Setup the stored procedure
            conn.query(
                R"(
                    CREATE PROCEDURE get_company(IN pin_company_id CHAR(10))
                    BEGIN
                        START TRANSACTION READ ONLY;
                        SELECT * FROM company WHERE id = pin_company_id;
                        SELECT * FROM employee WHERE company_id = pin_company_id;
                        COMMIT;
                    END
                )",
                result
            );

            // Get the company ID to retrieve, possibly from the user
            std::string company_id = get_company_id();

            // Call the procedure
            execution_state st;
            statement stmt = conn.prepare_statement("CALL get_company(?)");
            conn.start_statement_execution(stmt, std::make_tuple(company_id), st);

            // The above code will generate 3 resultsets
            // Read the 1st one, which contains the matched companies
            while (st.should_read_rows())
            {
                rows_view company_batch = conn.read_some_rows(st);
                // Use the retrieved companies as required
                //<-
                boost::ignore_unused(company_batch);
                //->
            }

            // Move on to the 2nd one, containing the employees for these companies
            conn.read_resultset_head(st);
            while (st.should_read_rows())
            {
                rows_view employee_batch = conn.read_some_rows(st);
                // Use the retrieved employees as required
                //<-
                boost::ignore_unused(employee_batch);
                //->
            }

            // The last one is an empty resultset containing information about the
            // CALL statement itself. We're not interested in this
            conn.read_resultset_head(st);
            assert(st.complete());
            //]
        }
    }

    // fields
    {
        //[fields_field_views
        results result;
        conn.query("SELECT 'Hello world!'", result);

        // fv doesn't own its memory; if result goes out of scope, fv becomes invalid
        field_view fv = result.rows().at(0).at(0);

        // sv also points into result; if result goes out of scope, sv becomes invalid
        string_view sv = fv.as_string();
        //]

        ASSERT(sv == "Hello world!");
    }
    {
        //[fields_field_views_scalars
        results result;
        conn.query("SELECT 42", result);

        // fv doesn't own its memory; if result goes out of scope, fv becomes invalid
        field_view fv = result.rows().at(0).at(0);

        // intv is valid even after result goes out of scope
        std::int64_t intv = fv.as_int64();
        //]

        ASSERT(intv == 42);
    }
    {
        //[fields_taking_ownership
        results result;
        conn.query("SELECT 'Hello world!'", result);

        // fv doesn't own its memory; if result goes out of scope, fv becomes invalid
        field_view fv = result.rows().at(0).at(0);

        // f takes ownership of fv's contents. f is valid even after result goes out of scope
        field f(fv);
        //]

        ASSERT(f.as_string() == "Hello world!");
    }
    {
        //[field_accessor_references
        field f("my_string");            // constructs a field that owns the string "my_string"
        std::string& s = f.as_string();  // s points into f's storage
        s.push_back('2');                // f now holds "my_string2"

        //]

        ASSERT(s == "my_string2");
    }
    {
        //[field_assignment
        field f("my_string");  // constructs a field that owns the string "my_string"
        f = 42;                // destroys "my_string" and stores the value 42 as an int64

        //]

        ASSERT(f.as_int64() == 42);
    }
    {
        //[field_date_as_time_point
        date d(2020, 2, 19);                      // d holds "2020-02-19"
        date::time_point tp = d.as_time_point();  // now use tp normally

        //]
        ASSERT(date(tp) == d);
    }
    {
        //[field_date_valid
        date d1(2020, 2, 19);  // regular date
        bool v1 = d1.valid();  // true
        date d2(2020, 0, 19);  // invalid date
        bool v2 = d2.valid();  // false

        //]
        ASSERT(v1);
        ASSERT(!v2);
    }
    {
        //[field_date_get_time_point
        date d = /* obtain a date somehow */ date(2020, 2, 29);
        if (d.valid())
        {
            // Same as as_time_point, but doesn't check for validity
            // Caution: be sure to check for validity.
            // If d is not valid, get_time_point results in undefined behavior
            date::time_point tp = d.get_time_point();

            // Use tp as required
            std::cout << tp.time_since_epoch().count() << std::endl;
        }
        else
        {
            // the date is invalid
            std::cout << "Invalid date" << std::endl;
        }
        //]
    }
    {
        //[field_datetime
        datetime dt1(2020, 10, 11, 10, 20, 59, 123456);  // regular datetime 2020-10-11 10:20:59.123456
        bool v1 = dt1.valid();                           // true
        datetime dt2(2020, 0, 11, 10, 20, 59);           // invalid datetime 2020-00-10 10:20:59.000000
        bool v2 = dt2.valid();                           // false

        datetime::time_point tp = dt1.as_time_point();  // convert to time_point

        //]
        ASSERT(v1);
        ASSERT(!v2);
        ASSERT(datetime(tp) == dt1);
    }
    {
        //[field_timestamp_setup
        results result;
        conn.query(
            R"%(
                CREATE TEMPORARY TABLE events (
                    id INT PRIMARY KEY AUTO_INCREMENT,
                    t TIMESTAMP,
                    contents VARCHAR(256)
                )
            )%",
            result
        );
        //]

        //[field_timestamp_stmts
        auto insert_stmt = conn.prepare_statement("INSERT INTO events (t, contents) VALUES (?, ?)");
        auto select_stmt = conn.prepare_statement("SELECT id, t, contents FROM events WHERE t > ?");
        //]

        //[fields_timestamp_set_time_zone
        // This change has session scope. All operations after this query
        // will now use UTC for TIMESTAMPs. Other sessions will not see the change.
        // If you need to reconnect the connection, you need to run this again.
        conn.query("SET @time_zone = 'UTC'", result);
        //]

        //[fields_timestamp_insert
        // Get the timestamp of the event. This may have been provided by an external system
        // For the sake of example, we will use the current timestamp
        datetime event_timestamp = datetime::now();

        // event_timestamp will be interpreted as UTC if you have run SET @time_zone
        conn.execute_statement(insert_stmt, std::make_tuple(event_timestamp, "Something happened"), result);
        //]

        //[fields_timestamp_select
        // Get the timestamp threshold from the user. We will use a constant for the sake of example
        datetime threshold = datetime(2022, 1, 1);  // get events that happened after 2022-01-01

        // threshold will be interpreted as UTC. The retrieved events will have their
        // `t` column in UTC
        conn.execute_statement(select_stmt, std::make_tuple(threshold), result);
        //]
    }
    {
        //[metadata
        // By default, a connection has metadata_mode::minimal
        results result;
        conn.query("SELECT 1 AS my_field", result);
        string_view colname = result.meta()[0].column_name();

        // colname will be empty because conn.meta_mode() == metadata_mode::minimal
        ASSERT(colname == "");

        // If you are using metadata names, set the connection's metadata_mode
        conn.set_meta_mode(metadata_mode::full);
        conn.query("SELECT 1 AS my_field", result);
        colname = result.meta()[0].column_name();
        ASSERT(colname == "my_field");
        //]
    }

    // Close
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
