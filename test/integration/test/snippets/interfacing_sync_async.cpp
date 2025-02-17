//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/config.hpp>
#ifndef BOOST_NO_CXX17_HDR_OPTIONAL

#include <boost/mysql/any_address.hpp>
#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/connection_pool.hpp>
#include <boost/mysql/field.hpp>
#include <boost/mysql/pool_params.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/statement.hpp>
#include <boost/mysql/static_results.hpp>
#include <boost/mysql/with_params.hpp>

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/cancel_after.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/config.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/describe/class.hpp>
#include <boost/system/system_error.hpp>
#include <boost/test/unit_test.hpp>

#include <cstdint>
#include <exception>
#include <future>
#include <optional>
#include <string>

#include "test_common/ci_server.hpp"
#include "test_integration/snippets/credentials.hpp"

namespace asio = boost::asio;
namespace mysql = boost::mysql;
using namespace mysql::test;

namespace {

struct employee
{
    std::string first_name;
    std::string last_name;
    std::optional<std::int64_t> salary;
};
BOOST_DESCRIBE_STRUCT(employee, (), (first_name, last_name, salary))

namespace v1 {

//[interfacing_sync_async_v1
// Gets an employee's name given their ID, using a connection pool. This is a sync function.
std::optional<employee> get_employee_by_id(mysql::connection_pool& pool, std::int64_t id)
{
    // Get a connection from the pool. This will launch the operation, but won't wait for it
    std::future<mysql::pooled_connection> fut = pool.async_get_connection(asio::use_future);

    // Block the current thread until the operation completes.
    // As we will explain later, you need a thread running your execution context for this to complete
    mysql::pooled_connection conn = fut.get();

    // There is a sync version of execute, so we can use it
    mysql::static_results<employee> r;
    conn->execute(mysql::with_params("SELECT * FROM employee WHERE id = {}", id), r);

    // Done
    return r.rows().empty() ? std::optional<employee>() : r.rows()[0];
}
//]

}  // namespace v1

namespace v2 {

BOOST_ATTRIBUTE_UNUSED
//[interfacing_sync_async_v2
std::optional<employee> get_employee_by_id(mysql::connection_pool& pool, std::int64_t id)
{
    using namespace std::chrono_literals;

    // Do NOT do this!! This is a race condition!!
    auto fut = pool.async_get_connection(asio::cancel_after(10s, asio::use_future));

    // ...
    //<-
    boost::ignore_unused(id);
    return {};
    //->
}
//]

}  // namespace v2

namespace v3 {

// clang-format off
//[interfacing_sync_async_v3
std::optional<employee> get_employee_by_id(mysql::connection_pool& pool, std::int64_t id)
{
    using namespace std::chrono_literals;

    // Create a strand for this operation. Strands require an underlying
    // executor. Use the pool's executor, which points to the thread_pool we created.
    auto strand = asio::make_strand(pool.get_executor());

    // First enter the strand with asio::dispatch, then call async_get_connection through the strand.
    // asio::dispatch + asio::bind_executor is Asio's standard way to "run a function in an executor"
    std::future<mysql::pooled_connection> fut = asio::dispatch(
        // bind_executor binds an executor to a completion token.
        // deferred creates an async chain
        asio::bind_executor(
            strand,
            asio::deferred([&] {
                // This function will be called when we're in the strand and determines what to do next
                return pool.async_get_connection(
                    asio::cancel_after(10s, asio::bind_executor(strand, asio::deferred))
                );
            })
        )
    )(asio::use_future); // Initiate the chain and convert it into a future

    // Wait for the async chain to finish
    mysql::pooled_connection conn = fut.get();

    // Execute as in the previous version
    mysql::static_results<employee> r;
    conn->execute(mysql::with_params("SELECT * FROM employee WHERE id = {}", id), r);
    return r.rows().empty() ? std::optional<employee>() : r.rows()[0];
}
//]
// clang-format on

}  // namespace v3

BOOST_AUTO_TEST_CASE(section_interfacing_sync_async_v1_v3)
{
    auto server_hostname = get_hostname();

    //[interfacing_sync_async_v1_init
    // Initialization code - run this once at program startup

    // Execution context, required to run all async operations.
    // This is equivalent to using asio::io_context and a thread that calls run()
    asio::thread_pool ctx(1);  // Use only one thread

    // Create the connection pool
    mysql::pool_params params;
    params.server_address.emplace_host_and_port(server_hostname);
    params.username = mysql_username;
    params.password = mysql_password;
    params.database = "boost_mysql_examples";
    params.thread_safe = true;  // allow initiating async_get_connection from any thread
    mysql::connection_pool pool(ctx, std::move(params));
    pool.async_run(asio::detached);
    //]

    // Check that everything's OK. v2 omitted because it's a race condition
    v1::get_employee_by_id(pool, 1);
    v3::get_employee_by_id(pool, 2);
}

#ifdef BOOST_ASIO_HAS_CO_AWAIT
namespace v4 {

//[interfacing_sync_async_v4
// Gets an employee's name given their ID, using a connection pool. This is a sync function.
std::optional<employee> get_employee_by_id(mysql::connection_pool& pool, std::int64_t id)
{
    using namespace std::chrono_literals;

    // Spawn a coroutine in the pool's executor - that is, in the thread_pool.
    // Since the pool has only one thread, and all code in the coroutine runs within that thread,
    // there is no need for a strand here.
    // co_spawn is an async operation, and can be used with asio::use_future
    std::future<std::optional<employee>> fut = asio::co_spawn(
        pool.get_executor(),
        [&pool, id]() -> asio::awaitable<std::optional<employee>> {
            // Get a connection from the pool
            auto conn = co_await pool.async_get_connection(asio::cancel_after(30s));

            // Execute
            mysql::static_results<employee> r;
            co_await conn->async_execute(
                mysql::with_params("SELECT * FROM employee WHERE id = {}", id),
                r,
                asio::cancel_after(30s)
            );

            // Done
            co_return r.rows().empty() ? std::optional<employee>() : r.rows()[0];
        },
        asio::use_future
    );

    // Wait for the future
    return fut.get();
}
//]

}  // namespace v4
#endif

namespace v5 {

//[interfacing_sync_async_v5
// Gets an employee's name given their ID, using a connection pool. This is a sync function.
std::optional<employee> get_employee_by_id(mysql::connection_pool& pool, std::int64_t id)
{
    using namespace std::chrono_literals;

    // A promise, so we can wait for the task to complete
    std::promise<std::optional<employee>> prom;

    // These temporary variables should be kept alive until all async operations
    // complete, so they're declared here
    mysql::pooled_connection conn;
    mysql::static_results<employee> r;

    // Ensure that everything runs within the thread pool
    asio::dispatch(asio::bind_executor(pool.get_executor(), [&] {
        // Get a connection from the pool
        pool.async_get_connection(asio::cancel_after(
            30s,
            [&](boost::system::error_code ec, mysql::pooled_connection temp_conn) {
                if (ec)
                {
                    // If there was an error getting the connection, complete the promise and return
                    prom.set_exception(std::make_exception_ptr(boost::system::system_error(ec)));
                }
                else
                {
                    // Store the connection somewhere. If it gets destroyed, it's returned to the pool
                    conn = std::move(temp_conn);

                    // Start executing the query
                    conn->async_execute(
                        mysql::with_params("SELECT * FROM employee WHERE id = {}", id),
                        r,
                        asio::cancel_after(
                            30s,
                            [&](boost::system::error_code ec) {
                                if (ec)
                                {
                                    // If there was an error, complete the promise and return
                                    prom.set_exception(std::make_exception_ptr(boost::system::system_error(ec)
                                    ));
                                }
                                else
                                {
                                    // Done
                                    prom.set_value(
                                        r.rows().empty() ? std::optional<employee>() : r.rows()[0]
                                    );
                                }
                            }
                        )
                    );
                }
            }
        ));
    }));

    return prom.get_future().get();
}
//]

}  // namespace v5

BOOST_AUTO_TEST_CASE(section_interfacing_sync_async_v4_v5)
{
    auto server_hostname = get_hostname();

    //[interfacing_sync_async_v4_init
    // Initialization code - run this once at program startup

    // Execution context, required to run all async operations.
    // This is equivalent to asio::io_context plus a thread calling run()
    asio::thread_pool ctx(1);

    // Create the connection pool. The pool is NOT thread-safe
    mysql::pool_params params;
    params.server_address.emplace_host_and_port(server_hostname);
    params.username = mysql_username;
    params.password = mysql_password;
    params.database = "boost_mysql_examples";
    mysql::connection_pool pool(ctx, std::move(params));

    // Run the pool. async_run should be executed in the thread_pool's thread -
    // otherwise, we have a race condition
    asio::dispatch(asio::bind_executor(ctx.get_executor(), [&] { pool.async_run(asio::detached); }));
//]

// Check that everything's OK
#ifdef BOOST_ASIO_HAS_CO_AWAIT
    v4::get_employee_by_id(pool, 0xfffff);
#endif
    v5::get_employee_by_id(pool, 1);
}

}  // namespace

#endif