//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/connection_pool.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/pool_params.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/ssl_mode.hpp>
#include <boost/mysql/string_view.hpp>
#include <boost/mysql/throw_on_error.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/test/unit_test.hpp>

#include <chrono>
#include <cstddef>
#include <memory>
#include <stdexcept>

#include "test_common/ci_server.hpp"
#include "test_common/create_diagnostics.hpp"
#include "test_common/printing.hpp"
#include "test_integration/run_stackful_coro.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;
namespace asio = boost::asio;

BOOST_AUTO_TEST_SUITE(test_connection_pool)

// For synchronization between tasks
class condition_variable
{
    asio::steady_timer timer_;

public:
    condition_variable(asio::any_io_executor ex)
        : timer_(std::move(ex), (std::chrono::steady_clock::time_point::max)())
    {
    }

    void notify() { timer_.expires_at((std::chrono::steady_clock::time_point::min)()); }

    void wait(asio::yield_context yield)
    {
        error_code ignored;
        timer_.async_wait(yield[ignored]);
    }
};

pool_params create_pool_params(std::size_t max_size = 151)
{
    pool_params res;
    res.server_address.emplace_host_and_port(get_hostname());
    res.username = integ_user;
    res.password = integ_passwd;
    res.database = integ_db;
    res.ssl = ssl_mode::disable;
    res.max_size = max_size;
    return res;
}

static void check_err(error_code ec) { BOOST_TEST(ec == error_code()); }

struct pool_deleter
{
    void operator()(connection_pool* pool) const { pool->cancel(); }
};

using pool_guard = std::unique_ptr<connection_pool, pool_deleter>;

struct fixture
{
    diagnostics diag{create_server_diag("diagnostics not cleared")};
    error_code ec{client_errc::server_unsupported};

    void check_success()
    {
        BOOST_TEST_REQUIRE(ec == error_code());
        BOOST_TEST(diag == diagnostics());
        ec = client_errc::server_unsupported;
        diag = create_server_diag("diagnostics not cleared");  // restore for successive ops
    }
};

// The pool and individual connections use the correct executors
BOOST_FIXTURE_TEST_CASE(pool_executors, fixture)
{
    run_stackful_coro([&](asio::yield_context yield) {
        // Create two different executors
        auto pool_ex = asio::make_strand(yield.get_executor());
        auto conn_ex = yield.get_executor();
        BOOST_TEST((pool_ex != conn_ex));

        // Create and run the pool
        connection_pool pool(pool_executor_params{pool_ex, conn_ex}, create_pool_params());
        pool_guard grd(&pool);
        pool.async_run(check_err);

        // Get a connection
        auto conn = pool.async_get_connection(diag, yield[ec]);
        check_success();

        // Check executors
        BOOST_TEST((pool.get_executor() == pool_ex));
        BOOST_TEST((conn->get_executor() == conn_ex));
    });
}

BOOST_FIXTURE_TEST_CASE(return_connection_with_reset, fixture)
{
    run_stackful_coro([&](asio::yield_context yield) {
        results r;

        // Create a pool with max_size 1, so the same connection gets always returned
        connection_pool pool(yield.get_executor(), create_pool_params(1));
        pool_guard grd(&pool);
        pool.async_run(check_err);

        // Get a connection
        auto conn = pool.async_get_connection(diag, yield[ec]);
        check_success();

        // Alter session state
        BOOST_TEST_REQUIRE(conn.valid());
        conn->async_execute("SET @myvar = 'abc'", r, diag, yield[ec]);
        check_success();

        // Return the connection
        conn = pooled_connection();

        // Get the same connection again
        conn = pool.async_get_connection(diag, yield[ec]);
        check_success();

        // The same connection is returned, but session state has been cleared
        BOOST_TEST_REQUIRE(conn.valid());
        conn->async_execute("SELECT @myvar", r, diag, yield[ec]);
        BOOST_TEST(r.rows().at(0).at(0) == field_view());
    });
}

BOOST_FIXTURE_TEST_CASE(return_connection_without_reset, fixture)
{
    run_stackful_coro([&](asio::yield_context yield) {
        results r;

        // Create a connection pool with max_size 1, so the same connection gets always returned
        connection_pool pool(yield.get_executor(), create_pool_params(1));
        pool_guard grd(&pool);
        pool.async_run(check_err);

        // Get a connection
        auto conn = pool.async_get_connection(diag, yield[ec]);
        check_success();

        // Alter session state
        BOOST_TEST_REQUIRE(conn.valid());
        conn->async_execute("SET @myvar = 'abc'", r, diag, yield[ec]);
        check_success();

        // Return the connection
        conn.return_without_reset();
        BOOST_TEST(!conn.valid());

        // Get the same connection again
        conn = pool.async_get_connection(diag, yield[ec]);
        check_success();

        // The same connection is returned, and no reset has been issued
        BOOST_TEST_REQUIRE(conn.valid());
        conn->async_execute("SELECT @myvar", r, diag, yield[ec]);
        BOOST_TEST(r.rows().at(0).at(0) == field_view("abc"));
    });
}

// pooled_connection destructor is equivalent to return_connection with reset
BOOST_FIXTURE_TEST_CASE(pooled_connection_destructor, fixture)
{
    run_stackful_coro([&](asio::yield_context yield) {
        results r;

        // Create a connection pool with max_size 1, so the same connection gets always returned
        connection_pool pool(yield.get_executor(), create_pool_params(1));
        pool_guard grd(&pool);
        pool.async_run(check_err);

        {
            // Get a connection
            auto conn = pool.async_get_connection(diag, yield[ec]);
            check_success();

            // Alter session state
            BOOST_TEST_REQUIRE(conn.valid());
            conn->async_execute("SET @myvar = 'abc'", r, diag, yield[ec]);
            check_success();
        }

        // Get the same connection again
        auto conn = pool.async_get_connection(diag, yield[ec]);
        check_success();

        // The same connection is returned, but session state has been cleared
        BOOST_TEST_REQUIRE(conn.valid());
        conn->async_execute("SELECT @myvar", r, diag, yield[ec]);
        BOOST_TEST(r.rows().at(0).at(0) == field_view());
    });
}

// Pooled connections use utf8mb4
static void validate_charset(any_connection& conn, asio::yield_context yield)
{
    // The connection knows its using utf8mb4
    BOOST_TEST(conn.current_character_set()->name == "utf8mb4");
    BOOST_TEST(conn.format_opts()->charset.name == "utf8mb4");

    // The connection is actually using utf8mb4
    results r;
    conn.async_execute(
        "SELECT @@character_set_client, @@character_set_connection, @@character_set_results",
        r,
        yield
    );
    const auto rw = r.rows().at(0);
    BOOST_TEST(rw.at(0).as_string() == "utf8mb4");
    BOOST_TEST(rw.at(1).as_string() == "utf8mb4");
    BOOST_TEST(rw.at(2).as_string() == "utf8mb4");
}

BOOST_FIXTURE_TEST_CASE(charset, fixture)
{
    run_stackful_coro([&](asio::yield_context yield) {
        // Create and run the pool
        connection_pool pool(yield.get_executor(), create_pool_params(1));
        pool_guard grd(&pool);
        pool.async_run(check_err);

        // Get a connection
        auto conn = pool.async_get_connection(yield);
        validate_charset(conn.get(), yield);

        // Return the connection and retrieve it again
        conn = pooled_connection();
        conn = pool.async_get_connection(yield);
        validate_charset(conn.get(), yield);

        // Return the connection without reset and retrieve it again
        conn.return_without_reset();
        conn = pool.async_get_connection(yield);
        validate_charset(conn.get(), yield);
    });
}

BOOST_FIXTURE_TEST_CASE(connections_created_if_required, fixture)
{
    run_stackful_coro([&](asio::yield_context yield) {
        results r;

        connection_pool pool(yield.get_executor(), create_pool_params());
        pool_guard grd(&pool);
        pool.async_run(check_err);

        // Get a connection
        auto conn1 = pool.async_get_connection(diag, yield[ec]);
        check_success();

        // Check that it works
        BOOST_TEST_REQUIRE(conn1.valid());
        conn1->async_execute("SET @myvar = '1'", r, diag, yield[ec]);
        check_success();

        // Get another connection. This will create a new one, since the first one is in use
        auto conn2 = pool.async_get_connection(diag, yield[ec]);
        check_success();

        // Check that it works
        BOOST_TEST_REQUIRE(conn1.valid());
        conn2->async_execute("SET @myvar = '2'", r, diag, yield[ec]);
        check_success();

        // They are different connections
        conn1->async_execute("SELECT @myvar", r, diag, yield[ec]);
        check_success();
        BOOST_TEST(r.rows().at(0).at(0) == field_view("1"));
        conn2->async_execute("SELECT @myvar", r, diag, yield[ec]);
        check_success();
        BOOST_TEST(r.rows().at(0).at(0) == field_view("2"));
    });
}

BOOST_FIXTURE_TEST_CASE(connection_upper_limit, fixture)
{
    run_stackful_coro([&](asio::yield_context yield) {
        results r;

        connection_pool pool(yield.get_executor(), create_pool_params(1));
        pool_guard grd(&pool);
        pool.async_run(check_err);

        // Get a connection
        auto conn = pool.async_get_connection(diag, yield[ec]);
        check_success();

        // Getting another connection will block until one is returned.
        // Since we won't return the one we have, the function time outs
        auto conn2 = pool.async_get_connection(std::chrono::milliseconds(1), diag, yield[ec]);
        BOOST_TEST(!conn2.valid());
        BOOST_TEST(ec == client_errc::timeout);
        BOOST_TEST(diag == diagnostics());
    });
}

BOOST_FIXTURE_TEST_CASE(cancel_run, fixture)
{
    run_stackful_coro([&](asio::yield_context yield) {
        results r;
        condition_variable run_cv(yield.get_executor());

        // Construct a pool and run it
        connection_pool pool(yield.get_executor(), create_pool_params());
        pool_guard grd(&pool);
        pool.async_run([&run_cv](error_code run_ec) {
            BOOST_TEST(run_ec == error_code());
            run_cv.notify();
        });

        // Get a connection
        auto conn = pool.async_get_connection(diag, yield[ec]);
        check_success();

        // Cancel. This will make run() return
        pool.cancel();
        run_cv.wait(yield);

        // Cancel again does nothing
        pool.cancel();
    });
}

// If the pool is cancelled before calling run, cancel still has effect
BOOST_FIXTURE_TEST_CASE(cancel_before_run, fixture)
{
    run_stackful_coro([&](asio::yield_context yield) {
        // Create a pool
        connection_pool pool(yield.get_executor(), create_pool_params());

        // Cancel
        pool.cancel();

        // Run returns immediately
        pool.async_run(check_err);
    });
}

BOOST_FIXTURE_TEST_CASE(cancel_get_connection, fixture)
{
    run_stackful_coro([&](asio::yield_context yield) {
        results r;
        condition_variable run_cv(yield.get_executor());
        condition_variable getconn_cv(yield.get_executor());

        // Construct a pool and run it
        connection_pool pool(yield.get_executor(), create_pool_params(1));
        pool_guard grd(&pool);
        pool.async_run([&run_cv](error_code run_ec) {
            BOOST_TEST(run_ec == error_code());
            run_cv.notify();
        });

        // Get a connection
        auto conn = pool.async_get_connection(diag, yield[ec]);
        check_success();

        // Try to get a new one. This will not complete, since there is no room for more connections
        pool.async_get_connection(diag, [&](error_code getconn_ec, pooled_connection conn2) {
            BOOST_TEST(getconn_ec == client_errc::cancelled);
            BOOST_TEST(!conn2.valid());
            getconn_cv.notify();
        });

        // Cancel. This will make run and get_connection return
        pool.cancel();
        run_cv.wait(yield);
        getconn_cv.wait(yield);

        // Calling get_connection after cancel will return client_errc::cancelled
        conn = pool.async_get_connection(diag, yield[ec]);
        BOOST_TEST(!conn.valid());
        BOOST_TEST(ec == client_errc::cancelled);
        BOOST_TEST(diag == diagnostics());
    });
}

BOOST_FIXTURE_TEST_CASE(get_connection_pool_not_running, fixture)
{
    run_stackful_coro([&](asio::yield_context yield) {
        // Create a pool but don't run it
        connection_pool pool(yield.get_executor(), create_pool_params());
        pool_guard grd(&pool);

        // Getting a connection fails immediately with a descriptive error code
        auto conn = pool.async_get_connection(diag, yield[ec]);
        BOOST_TEST(ec == client_errc::pool_not_running);
        BOOST_TEST(diag == diagnostics());
    });
}

// Having a valid pooled_connection alive extends the pool's lifetime
BOOST_FIXTURE_TEST_CASE(pooled_connection_extends_pool_lifetime, fixture)
{
    run_stackful_coro([&](asio::yield_context yield) {
        std::unique_ptr<connection_pool> pool(new connection_pool(yield.get_executor(), create_pool_params())
        );

        // Run the pool in a way we can synchronize with
        condition_variable run_cv(yield.get_executor());
        pool->async_run([&run_cv](error_code run_ec) {
            BOOST_TEST(run_ec == error_code());
            run_cv.notify();
        });

        // Get a connection
        auto conn = pool->async_get_connection(diag, yield[ec]);
        check_success();

        // Cancel and destroy
        pool->cancel();
        pool.reset();

        // Wait for run to exit, since run extends lifetime, too
        run_cv.wait(yield);

        // The connection we got can still be used and returned
        conn->async_ping(yield);
        conn.return_without_reset();
    });
}

// Having a packaged async_get_connection op extends lifetime
BOOST_FIXTURE_TEST_CASE(async_get_connection_initation_extends_pool_lifetime, fixture)
{
    run_stackful_coro([&](asio::yield_context yield) {
        std::unique_ptr<connection_pool> pool(new connection_pool(yield.get_executor(), create_pool_params())
        );

        // Create a packaged op
        auto op = pool->async_get_connection(diag, boost::asio::deferred);

        // Destroy the pool
        pool.reset();

        // We can run the operation without crashing, since it extends lifetime
        auto conn = op(yield[ec]);
        BOOST_TEST(ec == client_errc::pool_not_running);
        BOOST_TEST(diag == diagnostics());
    });
}

// Spotcheck: the different async_get_connection overloads work
BOOST_FIXTURE_TEST_CASE(get_connection_overloads, fixture)
{
    run_stackful_coro([&](asio::yield_context yield) {
        connection_pool pool(yield.get_executor(), create_pool_params());
        pool_guard grd(&pool);
        pool.async_run(check_err);

        // With all params
        auto conn = pool.async_get_connection(std::chrono::hours(1), diag, yield);
        conn->async_ping(yield);

        // With timeout, without diag
        conn = pool.async_get_connection(std::chrono::hours(1), yield);
        conn->async_ping(yield);

        // With diag, without timeout
        conn = pool.async_get_connection(diag, yield);
        conn->async_ping(yield);

        // Without diag, without timeout
        conn = pool.async_get_connection(yield);
        conn->async_ping(yield);
    });
}

// Spotcheck: async_get_connection timeouts work
BOOST_FIXTURE_TEST_CASE(get_connection_timeout, fixture)
{
    run_stackful_coro([&](asio::yield_context yield) {
        auto params = create_pool_params();
        params.password = "bad_password";  // Guarantee that no connection will ever become available
        connection_pool pool(yield.get_executor(), std::move(params));
        pool_guard grd(&pool);

        pool.async_run(check_err);

        // Getting a connection will timeout. The error may be a generic
        // timeout or a "bad password" error, depending on timing
        auto conn = pool.async_get_connection(std::chrono::milliseconds(1), diag, yield[ec]);
        BOOST_TEST(ec != error_code());
    });
}

// Spotcheck: pool works with unix sockets, too
BOOST_TEST_DECORATOR(*boost::unit_test::label("unix"))
BOOST_FIXTURE_TEST_CASE(unix_sockets, fixture)
{
    run_stackful_coro([&](asio::yield_context yield) {
        results r;
        auto params = create_pool_params();
        params.server_address.emplace_unix_path(default_unix_path);

        connection_pool pool(yield.get_executor(), std::move(params));
        pool_guard grd(&pool);
        pool.async_run(check_err);

        // Get a connection
        auto conn = pool.async_get_connection(diag, yield[ec]);
        check_success();

        // Verify that works
        BOOST_TEST_REQUIRE(conn.valid());
        conn->async_ping(yield);
    });
}

// Spotcheck: pool works with TLS
BOOST_FIXTURE_TEST_CASE(ssl, fixture)
{
    run_stackful_coro([&](asio::yield_context yield) {
        results r;
        auto params = create_pool_params();
        params.ssl = ssl_mode::require;

        connection_pool pool(yield.get_executor(), std::move(params));
        pool_guard grd(&pool);
        pool.async_run(check_err);

        // Get a connection
        auto conn = pool.async_get_connection(diag, yield[ec]);
        check_success();

        // Verify that works
        BOOST_TEST_REQUIRE(conn.valid());
        conn->async_ping(yield);
    });
}

// Spotcheck: custom ctor params (SSL context and buffer size) can be passed to the connection pool
BOOST_FIXTURE_TEST_CASE(custom_ctor_params, fixture)
{
    run_stackful_coro([&](asio::yield_context yield) {
        results r;
        auto params = create_pool_params();
        params.ssl = ssl_mode::require;
        params.ssl_ctx.emplace(asio::ssl::context::sslv23_client);
        params.initial_buffer_size = 16u;

        connection_pool pool(yield.get_executor(), std::move(params));
        pool_guard grd(&pool);
        pool.async_run(check_err);

        // Get a connection
        auto conn = pool.async_get_connection(diag, yield[ec]);
        check_success();

        // Verify that works
        BOOST_TEST_REQUIRE(conn.valid());
        conn->async_ping(yield);
    });
}

// Spotcheck: the pool can work with zero timeouts
BOOST_FIXTURE_TEST_CASE(zero_timeuts, fixture)
{
    run_stackful_coro([&](asio::yield_context yield) {
        results r;
        auto params = create_pool_params();
        params.max_size = 1u;  // so we force a reset
        params.connect_timeout = std::chrono::seconds(0);
        params.ping_timeout = std::chrono::seconds(0);
        params.ping_interval = std::chrono::seconds(0);

        connection_pool pool(yield.get_executor(), std::move(params));
        pool_guard grd(&pool);
        pool.async_run(check_err);

        // Get a connection
        auto conn = pool.async_get_connection(diag, yield[ec]);
        check_success();
        conn->async_ping(yield);

        // Return the connection
        conn = pooled_connection();

        // Get the same connection again. A zero timeout for async_get_connection works, too
        conn = pool.async_get_connection(std::chrono::seconds(0), diag, yield[ec]);
        check_success();
        conn->async_ping(yield);
    });
}

// Spotcheck: constructing a connection_pool with invalid params throws
BOOST_AUTO_TEST_CASE(invalid_params)
{
    asio::io_context ctx;
    pool_params params;
    params.connect_timeout = std::chrono::seconds(-1);

    BOOST_CHECK_EXCEPTION(
        connection_pool(ctx, std::move(params)),
        std::invalid_argument,
        [](const std::invalid_argument& exc) {
            return exc.what() == string_view("pool_params::connect_timeout must not be negative");
        }
    );
}

// Regression check: deferred works even in C++11
void deferred_check()
{
    asio::io_context ctx;
    connection_pool pool(ctx, pool_params());
    diagnostics diag;
    std::chrono::seconds timeout(5);

    (void)pool.async_run(asio::deferred);
    (void)pool.async_get_connection(timeout, diag, asio::deferred);
    (void)pool.async_get_connection(timeout, asio::deferred);
    (void)pool.async_get_connection(diag, asio::deferred);
    (void)pool.async_get_connection(asio::deferred);
}

BOOST_AUTO_TEST_SUITE_END()
