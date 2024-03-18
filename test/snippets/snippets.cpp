//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// This file contains all the snippets that are used in the docs.
// They're here so they are built and run, to ensure correctness

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/character_set.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/connection.hpp>
#include <boost/mysql/connection_pool.hpp>
#include <boost/mysql/date.hpp>
#include <boost/mysql/datetime.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/field.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/format_sql.hpp>
#include <boost/mysql/metadata_mode.hpp>
#include <boost/mysql/pool_params.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/resultset.hpp>
#include <boost/mysql/resultset_view.hpp>
#include <boost/mysql/row.hpp>
#include <boost/mysql/row_view.hpp>
#include <boost/mysql/rows.hpp>
#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/ssl_mode.hpp>
#include <boost/mysql/statement.hpp>
#include <boost/mysql/static_execution_state.hpp>
#include <boost/mysql/static_results.hpp>
#include <boost/mysql/string_view.hpp>
#include <boost/mysql/tcp_ssl.hpp>
#include <boost/mysql/throw_on_error.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/asio/ssl/verify_mode.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/config.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/core/span.hpp>
#include <boost/describe/class.hpp>
#include <boost/optional/optional.hpp>
#include <boost/system/system_error.hpp>

#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <tuple>

#include "test_common/ci_server.hpp"

#ifndef BOOST_NO_CXX17_HDR_OPTIONAL
#include <optional>
#endif
#ifdef BOOST_ASIO_HAS_CO_AWAIT
#include <boost/asio/experimental/awaitable_operators.hpp>
#endif
#ifdef __cpp_lib_polymorphic_allocator
#include <memory_resource>
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
#ifdef BOOST_MYSQL_CXX14
using boost::mysql::static_execution_state;
using boost::mysql::static_results;
#endif

#define ASSERT(expr)                                          \
    if (!(expr))                                              \
    {                                                         \
        std::cerr << "Assertion failed: " #expr << std::endl; \
        exit(1);                                              \
    }

// Connection params
std::string server_hostname = boost::mysql::test::get_hostname();
static constexpr const char* mysql_username = "example_user";
static constexpr const char* mysql_password = "example_password";

#ifdef BOOST_ASIO_HAS_CO_AWAIT
void run_coro(boost::asio::any_io_executor ex, std::function<boost::asio::awaitable<void>(void)> fn)
{
    boost::asio::co_spawn(ex, fn, [](std::exception_ptr ptr) {
        if (ptr)
        {
            std::rethrow_exception(ptr);
        }
    });
    static_cast<boost::asio::io_context&>(ex.context()).run();
}
#endif

//[any_connection_tcp
void create_and_connect(
    string_view server_hostname,
    string_view username,
    string_view password,
    string_view database
)
{
    // connect_params contains all the info required to establish a session
    boost::mysql::connect_params params;
    params.server_address.emplace_host_and_port(server_hostname);  // server host
    params.username = username;                                    // username to log in as
    params.password = password;                                    // password to use
    params.database = database;                                    // database to use

    // The execution context, required to run I/O operations.
    boost::asio::io_context ctx;

    // A connection to the server. Note how the type doesn't depend
    // on the transport being used.
    boost::mysql::any_connection conn(ctx);

    // Connect to the server. This will perform hostname resolution,
    // TCP-level connect, and the MySQL handshake. After this function
    // succeeds, your connection is ready to run queries
    conn.connect(params);
}
//]

// Intentionally not run, since it creates problems in Windows CIs
//[any_connection_unix
void create_and_connect_unix(string_view username, string_view password, string_view database)
{
    // server_address may contain a UNIX socket path, too
    boost::mysql::connect_params params;
    params.server_address.emplace_unix_path("/var/run/mysqld/mysqld.sock");
    params.username = username;  // username to log in as
    params.password = password;  // password to use
    params.database = database;  // database to use

    // The execution context, required to run I/O operations.
    boost::asio::io_context ctx;

    // A connection to the server. Note how the type doesn't depend
    // on the transport being used.
    boost::mysql::any_connection conn(ctx);

    // Connect to the server. This will perform the
    // UNIX socket connect and the MySQL handshake. After this function
    // succeeds, your connection is ready to run queries
    conn.connect(params);
}
//]

//[any_connection_reconnect
error_code connect_with_retries(
    boost::mysql::any_connection& conn,
    const boost::mysql::connect_params& params
)
{
    // We will be using the non-throwing overloads
    error_code ec;
    diagnostics diag;

    // Try to connect at most 10 times
    for (int i = 0; i < 10; ++i)
    {
        // Try to connect
        conn.connect(params, ec, diag);

        // If we succeeded, we're done
        if (!ec)
            return error_code();

        // Whoops, connect failed. We can sleep and try again
        std::cerr << "Failed connecting to MySQL: " << ec << ": " << diag.server_message() << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // No luck, retries expired
    return ec;
}
//]

void section_any_connection()
{
    create_and_connect(server_hostname, mysql_username, mysql_password, "boost_mysql_examples");

    {
        boost::mysql::connect_params params;
        params.server_address.emplace_host_and_port(server_hostname);  // server host
        params.username = mysql_username;                              // username to log in as
        params.password = mysql_password;                              // password to use

        boost::asio::io_context ctx;
        boost::mysql::any_connection conn(ctx);
        auto ec = connect_with_retries(conn, params);
        ASSERT(ec == error_code());
    }

    {
        boost::mysql::connect_params params;

        //[any_connection_ssl_mode
        // Don't ever use TLS, even if the server supports it
        params.ssl = boost::mysql::ssl_mode::disable;

        // ...

        // Force using TLS. If the server doesn't support it, reject the connection
        params.ssl = boost::mysql::ssl_mode::require;
        //]
    }

    {
        //[any_connection_ssl_ctx
        // The I/O context requied to run network operations
        boost::asio::io_context ctx;

        // Create a SSL context
        boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::tlsv12_client);

        // Set options on the SSL context. Load the default certificate authorities
        // and enable certificate verification. connect will fail if the server certificate
        // isn't signed by a trusted entity or its hostname isn't "mysql"
        ssl_ctx.set_default_verify_paths();
        ssl_ctx.set_verify_mode(boost::asio::ssl::verify_peer);
        ssl_ctx.set_verify_callback(boost::asio::ssl::host_name_verification("mysql"));

        // Construct an any_connection object passing the SSL context.
        // You must keep ssl_ctx alive while using the connection.
        boost::mysql::any_connection_params ctor_params;
        ctor_params.ssl_context = &ssl_ctx;
        boost::mysql::any_connection conn(ctx, ctor_params);

        // Connect params
        boost::mysql::connect_params params;
        params.server_address.emplace_host_and_port(server_hostname);  // server host
        params.username = mysql_username;                              // username to log in as
        params.password = mysql_password;                              // password to use
        params.ssl = boost::mysql::ssl_mode::require;                  // fail if TLS is not available

        // Connect
        error_code ec;
        diagnostics diag;
        conn.connect(params, ec, diag);
        if (ec)
        {
            // Handle error
        }
        //]
        ASSERT(ec != error_code());
        ASSERT(ec.category() == boost::asio::error::get_ssl_category());
    }
}

#ifdef BOOST_ASIO_HAS_CO_AWAIT
//[connection_pool_get_connection
// Use connection pools for functions that will be called
// repeatedly during the application lifetime.
// An HTTP server handler function is a good candidate.
boost::asio::awaitable<std::int64_t> get_num_employees(boost::mysql::connection_pool& pool)
{
    // Get a fresh connection from the pool.
    // pooled_connection is a proxy to an any_connection object.
    boost::mysql::pooled_connection conn = co_await pool.async_get_connection(boost::asio::use_awaitable);

    // Use pooled_connection::operator-> to access the underlying any_connection.
    // Let's use the connection
    results result;
    co_await conn->async_execute("SELECT COUNT(*) FROM employee", result, boost::asio::use_awaitable);
    co_return result.rows().at(0).at(0).as_int64();

    // When conn is destroyed, the connection is returned to the pool
}
//]

boost::asio::awaitable<void> return_without_reset(boost::mysql::connection_pool& pool)
{
    //[connection_pool_return_without_reset
    // Get a connection from the pool
    boost::mysql::pooled_connection conn = co_await pool.async_get_connection(boost::asio::use_awaitable);

    // Use the connection in a way that doesn't mutate session state.
    // We're not setting variables, preparing statements or starting transactions,
    // so it's safe to skip reset
    boost::mysql::results result;
    co_await conn->async_execute("SELECT COUNT(*) FROM employee", result, boost::asio::use_awaitable);

    // Explicitly return the connection to the pool, skipping reset
    conn.return_without_reset();
    //]
}
#endif

//[connection_pool_sync
// Wraps a connection_pool and offers a sync interface.
// sync_pool is thread-safe
class sync_pool
{
    // A thread pool with a single thread. This is used to
    // run the connection pool. The thread is automatically
    // joined when sync_pool is destroyed.
    boost::asio::thread_pool thread_pool_{1};

    // The async connection pool
    boost::mysql::connection_pool conn_pool_;

public:
    // Constructor: constructs the connection_pool object from
    // the single-thread pool and calls async_run.
    // The pool has a single thread, which creates an implicit strand.
    // There is no need to use pool_executor_params::thread_safe
    sync_pool(boost::mysql::pool_params params) : conn_pool_(thread_pool_, std::move(params))
    {
        // Run the pool in the background (this is performed by the thread_pool thread).
        // When sync_pool is destroyed, this task will be stopped and joined automatically.
        conn_pool_.async_run(boost::asio::detached);
    }

    // Retrieves a connection from the pool (error code version)
    boost::mysql::pooled_connection get_connection(
        boost::mysql::error_code& ec,
        boost::mysql::diagnostics& diag,
        std::chrono::steady_clock::duration timeout = std::chrono::seconds(30)
    )
    {
        // The completion token to use for the async initiation function.
        // use_future will make the async function return a std::future object, which will
        // become ready when the operation completes.
        // as_tuple prevents the future from throwing on error, and packages the result as a tuple.
        // The returned future will be std::future<std::tuple<error_code, pooled_connection>>.
        constexpr auto completion_token = boost::asio::as_tuple(boost::asio::use_future);

        // We will use std::tie to decompose the tuple into its components.
        // We need to declare the connection before using std::tie
        boost::mysql::pooled_connection res;

        // async_get_connection returns a future. Calling std::future::get will
        // wait for the future to become ready
        std::tie(ec, res) = conn_pool_.async_get_connection(timeout, diag, completion_token).get();

        // Done!
        return res;
    }

    // Retrieves a connection from the pool (exception version)
    boost::mysql::pooled_connection get_connection(
        std::chrono::steady_clock::duration timeout = std::chrono::seconds(30)
    )
    {
        // Call the error code version
        boost::mysql::error_code ec;
        boost::mysql::diagnostics diag;
        auto res = get_connection(ec, diag, timeout);

        // This will throw boost::mysql::error_with_diagnostics on error
        boost::mysql::throw_on_error(ec, diag);

        // Done
        return res;
    }
};
//]

void section_connection_pool()
{
    {
        //[connection_pool_create
        // pool_params contains configuration for the pool.
        // You must specify enough information to establish a connection,
        // including the server address and credentials.
        // You can configure a lot of other things, like pool limits
        boost::mysql::pool_params params;
        params.server_address.emplace_host_and_port(server_hostname);
        params.username = mysql_username;
        params.password = mysql_password;
        params.database = "boost_mysql_examples";

        // The I/O context, required by all I/O operations
        boost::asio::io_context ctx;

        // Construct a pool of connections. The context will be used internally
        // to create the connections and other I/O objects
        boost::mysql::connection_pool pool(ctx, std::move(params));

        // You need to call async_run on the pool before doing anything useful with it.
        // async_run creates connections and keeps them healthy. It must be called
        // only once per pool.
        // The detached completion token means that we don't want to be notified when
        // the operation ends. It's similar to a no-op callback.
        pool.async_run(boost::asio::detached);
        //]

#ifdef BOOST_ASIO_HAS_CO_AWAIT
        run_coro(ctx.get_executor(), [&pool]() -> boost::asio::awaitable<void> {
            co_await get_num_employees(pool);
            pool.cancel();
        });
#endif
    }
    {
        boost::asio::io_context ctx;

        //[connection_pool_configure_size
        boost::mysql::pool_params params;

        // Set the usual params
        params.server_address.emplace_host_and_port(server_hostname);
        params.username = mysql_username;
        params.password = mysql_password;
        params.database = "boost_mysql_examples";

        // Create 10 connections at startup, and allow up to 1000 connections
        params.initial_size = 10;
        params.max_size = 1000;

        boost::mysql::connection_pool pool(ctx, std::move(params));
        //]

#ifdef BOOST_ASIO_HAS_CO_AWAIT
        pool.async_run(boost::asio::detached);
        run_coro(ctx.get_executor(), [&pool]() -> boost::asio::awaitable<void> {
            co_await return_without_reset(pool);
            pool.cancel();
        });
#endif
    }
    {
        //[connection_pool_thread_safe
        // The I/O context, required by all I/O operations
        boost::asio::io_context ctx;

        // The usual pool configuration params
        boost::mysql::pool_params params;
        params.server_address.emplace_host_and_port(server_hostname);
        params.username = mysql_username;
        params.password = mysql_password;
        params.database = "boost_mysql_examples";

        // By passing pool_executor_params::thread_safe to connection_pool,
        // we make all its member functions thread-safe.
        // This works by creating a strand.
        boost::mysql::connection_pool pool(
            boost::mysql::pool_executor_params::thread_safe(ctx.get_executor()),
            std::move(params)
        );

        // We can now pass a reference to pool to other threads,
        // and call async_get_connection concurrently without problem.
        // Inidivudal connections are still not thread-safe.
        //]
    }
    {
        boost::mysql::pool_params params;
        params.server_address.emplace_host_and_port(server_hostname);
        params.username = mysql_username;
        params.password = mysql_password;
        params.database = "boost_mysql_examples";

        sync_pool spool(std::move(params));

        auto conn1 = spool.get_connection();
        ASSERT(conn1.valid());
    }
}

static std::string get_name() { return "John"; }

#ifndef BOOST_NO_CXX17_HDR_OPTIONAL
//[sql_formatting_incremental_fn
// Compose an update query that sets first_name, last_name, or both
std::string compose_update_query(
    boost::mysql::format_options opts,
    std::int64_t employee_id,
    std::optional<std::string> new_first_name,
    std::optional<std::string> new_last_name
)
{
    // There should be at least one update
    assert(new_first_name || new_last_name);

    // format_context will accumulate the query as we compose it
    boost::mysql::format_context ctx(opts);

    // append_raw adds raw SQL to the generated query, without quoting or escaping.
    // You can only pass strings known at compile-time to append_raw,
    // unless you use the runtime function.
    ctx.append_raw("UPDATE employee SET ");

    if (new_first_name)
    {
        // format_sql_to expands a format string and appends the result
        // to a format context. This way, we can build our query in small pieces
        // Add the first_name update clause
        boost::mysql::format_sql_to(ctx, "first_name = {}", *new_first_name);
    }
    if (new_last_name)
    {
        if (new_first_name)
            ctx.append_raw(", ");

        // Add the last_name update clause
        boost::mysql::format_sql_to(ctx, "last_name = {}", *new_last_name);
    }

    // Add the where clause
    boost::mysql::format_sql_to(ctx, " WHERE id = {}", employee_id);

    // Retrieve the generated query string
    return std::move(ctx).get().value();
}
//]
#endif

//[sql_formatting_formatter_specialization
// We want to add formatting support for employee_t
struct employee_t
{
    std::string first_name;
    std::string last_name;
    std::string company_id;
};

namespace boost {
namespace mysql {

template <>
struct formatter<employee_t>
{
    // formatter<T> should define, at least, a function with signature:
    //    static void format(const T&, format_context_base&)
    // This function must use format_sql_to, format_context_base::append_raw
    // or format_context_base::append_value to format the passed value.
    // We will make this suitable for INSERT statements
    static void format(const employee_t& emp, format_context_base& ctx)
    {
        format_sql_to(ctx, "{}, {}, {}", emp.first_name, emp.last_name, emp.company_id);
    }
};

}  // namespace mysql
}  // namespace boost
//]

#ifndef BOOST_NO_CXX17_HDR_OPTIONAL
//[sql_formatting_unit_test
// For reference, the function under test
std::string compose_update_query(
    boost::mysql::format_options opts,
    std::int64_t employee_id,
    std::optional<std::string> new_first_name,
    std::optional<std::string> new_last_name
);

// Your test body
void test_compose_update_query()
{
    // You can safely use these format_options for testing,
    // since they are the most common ones.
    boost::mysql::format_options opts{boost::mysql::utf8mb4_charset, true};

    // Test for the different cases
    ASSERT(
        compose_update_query(opts, 42, "Bob", {}) == "UPDATE employee SET first_name = 'Bob' WHERE id = 42"
    );
    ASSERT(
        compose_update_query(opts, 42, {}, "Alice") == "UPDATE employee SET last_name = 'Alice' WHERE id = 42"
    );
    ASSERT(
        compose_update_query(opts, 0, "Bob", "Alice") ==
        "UPDATE employee SET first_name = 'Bob', last_name = 'Alice' WHERE id = 0"
    );
}
//]
#endif

void section_sql_formatting()
{
    boost::mysql::connect_params params;
    params.server_address.emplace_host_and_port(server_hostname);
    params.username = mysql_username;
    params.password = mysql_password;
    params.database = "boost_mysql_examples";
    params.multi_queries = true;
    params.ssl = boost::mysql::ssl_mode::disable;

    boost::asio::io_context ioc;
    boost::mysql::any_connection conn(ioc);
    boost::mysql::results r;

    conn.connect(params);

    {
        //[sql_formatting_simple
        std::string employee_name = get_name();  // employee_name is an untrusted string

        // Compose the SQL query in the client
        std::string query = boost::mysql::format_sql(
            conn.format_opts().value(),
            "SELECT id, salary FROM employee WHERE last_name = {}",
            employee_name
        );

        // If employee_name is "John", query now contains:
        // "SELECT id, salary FROM employee WHERE last_name = 'John'"
        // If employee_name contains quotes, they will be escaped as required

        // Execute the generated query as usual
        results result;
        conn.execute(query, result);
        //]

        ASSERT(query == "SELECT id, salary FROM employee WHERE last_name = 'John'");
    }
    {
        //[sql_formatting_other_scalars
        std::string query = boost::mysql::format_sql(
            conn.format_opts().value(),
            "SELECT id FROM employee WHERE salary > {}",
            42000
        );

        ASSERT(query == "SELECT id FROM employee WHERE salary > 42000");
        //]

        conn.execute(query, r);
    }
#ifndef BOOST_NO_CXX17_HDR_OPTIONAL
    {
        //[sql_formatting_optionals
        std::optional<std::int64_t> salary;  // get salary from a possibly untrusted source

        std::string query = boost::mysql::format_sql(
            conn.format_opts().value(),
            "UPDATE employee SET salary = {} WHERE id = {}",
            salary,
            1
        );

        // Depending on whether salary has a value or not, generates:
        // UPDATE employee SET salary = 42000 WHERE id = 1
        // UPDATE employee SET salary = NULL WHERE id = 1
        //]

        ASSERT(query == "UPDATE employee SET salary = NULL WHERE id = 1");
        conn.execute(query, r);
    }
#endif
    {
        //[sql_formatting_manual_indices
        // Recall that you need to set connect_params::multi_queries to true when connecting
        // before running semicolon-separated queries.
        std::string query = boost::mysql::format_sql(
            conn.format_opts().value(),
            "UPDATE employee SET first_name = {1} WHERE id = {0}; SELECT * FROM employee WHERE id = {0}",
            42,
            "John"
        );

        ASSERT(
            query ==
            "UPDATE employee SET first_name = 'John' WHERE id = 42; SELECT * FROM employee WHERE id = 42"
        );
        //]

        conn.execute(query, r);
    }
    {
        // clang-format off
        //[sql_formatting_named_args
        std::string query = boost::mysql::format_sql(
            conn.format_opts().value(),
            "UPDATE employee SET first_name = {name} WHERE id = {id}; SELECT * FROM employee WHERE id = {id}",
            {
                {"id",   42    },
                {"name", "John"}
            }
        );
        //<-
        // clang-format on
        //->

        ASSERT(
            query ==
            "UPDATE employee SET first_name = 'John' WHERE id = 42; SELECT * FROM employee WHERE id = 42"
        );
        //]

        conn.execute(query, r);
    }
    {
        //[sql_formatting_identifiers
        std::string query = boost::mysql::format_sql(
            conn.format_opts().value(),
            "SELECT id, last_name FROM employee ORDER BY {} DESC",
            boost::mysql::identifier("company_id")
        );

        ASSERT(query == "SELECT id, last_name FROM employee ORDER BY `company_id` DESC");
        //]

        conn.execute(query, r);
    }
    {
        //[sql_formatting_qualified_identifiers
        std::string query = boost::mysql::format_sql(
            conn.format_opts().value(),
            "SELECT salary, tax_id FROM employee "
            "INNER JOIN company ON employee.company_id = company.id "
            "ORDER BY {} DESC",
            boost::mysql::identifier("company", "id")
        );
        // SELECT ... ORDER BY `company`.`id` DESC
        //]

        ASSERT(
            query ==
            "SELECT salary, tax_id FROM employee "
            "INNER JOIN company ON employee.company_id = company.id "
            "ORDER BY `company`.`id` DESC"
        );
        conn.execute(query, r);
    }
#ifndef BOOST_NO_CXX17_HDR_OPTIONAL
    {
        //[sql_formatting_incremental_use
        std::string query = compose_update_query(conn.format_opts().value(), 42, "John", {});

        ASSERT(query == "UPDATE employee SET first_name = 'John' WHERE id = 42");
        //]

        conn.execute(query, r);
    }
    {
        test_compose_update_query();
    }
#endif
    {
        try
        {
            //[sql_formatting_invalid_encoding
            // If the connection is using UTF-8 (the default), this will throw an error,
            // because the string to be formatted contains invalid UTF8.
            format_sql(conn.format_opts().value(), "SELECT {}", "bad\xff UTF-8");
            //]

            ASSERT(false);
        }
        catch (const boost::system::system_error& err)
        {
            ASSERT(err.code() == boost::mysql::client_errc::invalid_encoding);
        }
    }
    {
        using namespace boost::mysql;
        using boost::optional;
        const auto opts = conn.format_opts().value();

        //[sql_formatting_reference_signed
        //<-
        ASSERT(
            //->
            format_sql(opts, "SELECT {}", 42) == "SELECT 42"
            //<-
        );
        //->
        //<-
        ASSERT(
            //->
            format_sql(opts, "SELECT {}", -1) == "SELECT -1"
            //<-
        );
        //->
        //]

        //[sql_formatting_reference_unsigned
        //<-
        ASSERT(
            //->
            format_sql(opts, "SELECT {}", 42u) == "SELECT 42"
            //<-
        );
        //->
        //]

        //[sql_formatting_reference_bool
        //<-
        ASSERT(
            //->
            format_sql(opts, "SELECT {}", false) == "SELECT 0"
            //<-
        );
        //->
        //<-
        ASSERT(
            //->
            format_sql(opts, "SELECT {}", true) == "SELECT 1"
            //<-
        );
        //->
        //]

        //[sql_formatting_reference_string
        //<-
        ASSERT(
            //->
            format_sql(opts, "SELECT {}", "Hello world") == "SELECT 'Hello world'"
            //<-
        );
        //->
        //<-
        ASSERT(
            //->
            format_sql(opts, "SELECT {}", "Hello 'world'") == R"(SELECT 'Hello \'world\'')"
            //<-
        );
        //->
        //]

        //[sql_formatting_reference_blob
        //<-
        ASSERT(
            //->
            format_sql(opts, "SELECT {}", blob{0x00, 0x48, 0xff}) == R"(SELECT x'0048ff')"
            //<-
        );
        //->
        //]

        //[sql_formatting_reference_float
        //<-
        ASSERT(
            //->
            // Equivalent to format_sql(opts, "SELECT {}", static_cast<double>(4.2f))
            // Note that MySQL uses doubles for all floating point literals
            format_sql(opts, "SELECT {}", 4.2f) == "SELECT 4.199999809265137e+00"
            //<-
        );
        //->
        //]

        //[sql_formatting_reference_double
        //<-
        ASSERT(
            //->
            format_sql(opts, "SELECT {}", 4.2) == "SELECT 4.2e+00"
            //<-
        );
        //->
        //]

        //[sql_formatting_reference_date
        //<-
        ASSERT(
            //->
            format_sql(opts, "SELECT {}", date(2021, 1, 2)) == "SELECT '2021-01-02'"
            //<-
        );
        //->
        //]

        //[sql_formatting_reference_datetime
        //<-
        ASSERT(
            // clang-format off
            //->
            format_sql(opts, "SELECT {}", datetime(2021, 1, 2, 23, 51, 14)) == "SELECT '2021-01-02 23:51:14.000000'"
            //<-
            // clang-format on
        );
        //->
        //]

        //[sql_formatting_reference_time
        //<-
        ASSERT(
            //->
            format_sql(opts, "SELECT {}", std::chrono::seconds(121)) == "SELECT '00:02:01.000000'"
            //<-
        );
        //->
        //]

        //[sql_formatting_reference_nullptr
        //<-
        ASSERT(
            //->
            format_sql(opts, "SELECT {}", nullptr) == "SELECT NULL"
            //<-
        );
        //->
        //]

        //[sql_formatting_reference_optional
        //<-
        ASSERT(
            //->
            format_sql(opts, "SELECT {}", optional<int>(42)) == "SELECT 42"
            //<-
        );
        //->
        //<-
        ASSERT(
            //->
            format_sql(opts, "SELECT {}", optional<int>()) == "SELECT NULL"
            //<-
        );
        //->
        //]

        //[sql_formatting_reference_field
        //<-
        ASSERT(
            //->
            format_sql(opts, "SELECT {}", field(42)) == "SELECT 42"
            //<-
        );
        //->
        //<-
        ASSERT(
            //->
            format_sql(opts, "SELECT {}", field("abc")) == "SELECT 'abc'"
            //<-
        );
        //->
        //<-
        ASSERT(
            //->
            format_sql(opts, "SELECT {}", field()) == "SELECT NULL"
            //<-
        );
        //->
        //]

        //[sql_formatting_reference_identifier
        //<-
        ASSERT(
            //->
            format_sql(opts, "SELECT {} FROM t", identifier("salary")) == "SELECT `salary` FROM t"
            //<-
        );
        //->
        //<-
        ASSERT(
            //->
            format_sql(opts, "SELECT {} FROM t", identifier("sal`ary")) == "SELECT `sal``ary` FROM t"
            //<-
        );
        //->
        //<-
        ASSERT(
            // clang-format off
            //->
            format_sql(opts, "SELECT {} FROM t", identifier("mytable", "myfield")) == "SELECT `mytable`.`myfield` FROM t"
            //<-
            // clang-format on
        );
        //->
        //<-
        ASSERT(
            // clang-format off
            //->
            format_sql(opts, "SELECT {} FROM t", identifier("mydb", "mytable", "myfield")) == "SELECT `mydb`.`mytable`.`myfield` FROM t"
            //<-
            // clang-format on
        );
        //->
        //]
    }

    // Advanced section
    {
        //[sql_formatting_formatter_use
        // We can now use employee as a built-in value
        std::string query = boost::mysql::format_sql(
            conn.format_opts().value(),
            "INSERT INTO employee (first_name, last_name, company_id) VALUES ({}), ({})",
            employee_t{"John", "Doe", "HGS"},
            employee_t{"Rick", "Johnson", "AWC"}
        );

        ASSERT(
            query ==
            "INSERT INTO employee (first_name, last_name, company_id) VALUES "
            "('John', 'Doe', 'HGS'), ('Rick', 'Johnson', 'AWC')"
        );
        //]

        conn.execute(query, r);
    }
    {
        const auto opts = conn.format_opts().value();

        //[sql_formatting_auto_indexing
        ASSERT(format_sql(opts, "SELECT {}, {}, {}", 42, "abc", nullptr) == "SELECT 42, 'abc', NULL");
        //]
    }
    {
        const auto opts = conn.format_opts().value();

        //[sql_formatting_manual_auto_mix
        try
        {
            // Mixing manual and auto indexing is illegal. This will throw an exception.
            format_sql(opts, "SELECT {0}, {}", 42);
            //<-
            ASSERT(false);
            //->
        }
        catch (const boost::system::system_error& err)
        {
            ASSERT(err.code() == boost::mysql::client_errc::format_string_manual_auto_mix);
        }
        //]
    }
    {
        const auto opts = conn.format_opts().value();

        //[sql_formatting_unused_args
        // This is OK
        std::string query = format_sql(opts, "SELECT {}", 42, "abc");
        //]
        ASSERT(query == "SELECT 42");
    }
    {
        const auto opts = conn.format_opts().value();

        //[sql_formatting_brace_literal
        ASSERT(format_sql(opts, "SELECT 'Brace literals: {{ and }}'") == "SELECT 'Brace literals: { and }'");
        //]
    }
    {
        const auto opts = conn.format_opts().value();

        //[sql_formatting_format_double_error
        try
        {
            // We're trying to format a double infinity value, which is not
            // supported by MySQL. This will throw an exception.
            std::string formatted_query = format_sql(opts, "SELECT {}", HUGE_VAL);
            //<-
            ASSERT(false);
            boost::ignore_unused(formatted_query);
            //->
        }
        catch (const boost::system::system_error& err)
        {
            ASSERT(err.code() == boost::mysql::client_errc::unformattable_value);
        }
        //]
    }
    {
        const auto opts = conn.format_opts().value();

        //[sql_formatting_no_exceptions
        // ctx contains an error code that tracks whether any error happened
        boost::mysql::format_context ctx(opts);

        // We're trying to format a infinity, which is an error. This
        // will set the error state, but won't throw.
        format_sql_to(ctx, "SELECT {}, {}", HUGE_VAL, 42);

        // The error state gets checked at this point. Since it is set,
        // res will contain an error.
        boost::system::result<std::string> res = std::move(ctx).get();
        ASSERT(!res.has_value());
        ASSERT(res.has_error());
        ASSERT(res.error() == boost::mysql::client_errc::unformattable_value);
        // res.value() would throw an error, like format_sql would
        //]
    }
#ifdef __cpp_lib_polymorphic_allocator
    {
        //[sql_formatting_custom_string
        // Create a format context that uses std::pmr::string
        boost::mysql::basic_format_context<std::pmr::string> ctx(conn.format_opts().value());

        // Compose your query as usual
        boost::mysql::format_sql_to(ctx, "SELECT * FROM employee WHERE id = {}", 42);

        // Retrieve the query as usual
        std::pmr::string query = std::move(ctx).get().value();
        //]

        ASSERT(query == "SELECT * FROM employee WHERE id = 42");
        conn.execute(query, r);
    }
#endif
    {
        //[sql_formatting_memory_reuse
        // we want to re-use memory held by storage
        std::string storage;

        // storage is moved into ctx by the constructor. If any memory
        // had been allocated by the string, it will be re-used.
        boost::mysql::format_context ctx(conn.format_opts().value(), std::move(storage));

        // Use ctx as you normally would
        boost::mysql::format_sql_to(ctx, "SELECT {}", 42);

        // When calling get(), the string is moved out of the context
        std::string query = std::move(ctx).get().value();
        //]

        ASSERT(query == "SELECT 42");
    }
}

void main_impl()
{
    //
    // setup and connect - this is included in overview, too
    //

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
    auto endpoints = resolver.resolve(server_hostname, boost::mysql::default_port_string);

    // The username and password to use
    boost::mysql::handshake_params params(
        mysql_username,         // username
        mysql_password,         // password
        "boost_mysql_examples"  // database
    );

    // Connect to the server using the first endpoint returned by the resolver
    conn.connect(*endpoints.begin(), params);
    //]

    section_multi_resultset(conn);
    section_multi_resultset_multi_queries();
    section_multi_function(conn);
    section_metadata(conn);
    section_charsets(conn);
    section_time_types(conn);
    section_any_connection();
    section_connection_pool();
    section_sql_formatting();

    conn.close();
}

int main()
{
    try
    {
        main_impl();
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
