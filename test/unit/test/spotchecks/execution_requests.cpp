//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Since integration tests can't reliably test multifunction operations
// that span over multiple messages, we test the complete multifn fllow in this unit tests.

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/character_set.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/string_view.hpp>
#include <boost/mysql/tcp.hpp>
#include <boost/mysql/with_params.hpp>

#include <boost/mysql/detail/access.hpp>

#include <boost/asio/deferred.hpp>
#include <boost/asio/detached.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_suite.hpp>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <vector>

#include "test_common/assert_buffer_equals.hpp"
#include "test_common/network_result.hpp"
#include "test_common/tracker_executor.hpp"
#include "test_unit/create_ok.hpp"
#include "test_unit/create_ok_frame.hpp"
#include "test_unit/create_query_frame.hpp"
#include "test_unit/test_any_connection.hpp"
#include "test_unit/test_stream.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;
namespace asio = boost::asio;

BOOST_AUTO_TEST_SUITE(test_execution_requests)

// The execution request is forwarded correctly, and non-const objects work correctly.
// This is relevant for with_params involving ranges::views::filter, for instance
// Note: netmakers intentionally not used here
BOOST_AUTO_TEST_SUITE(forwarding_constness)

struct fixture
{
    any_connection conn{create_test_any_connection()};
    results result;
    execution_state st;

    fixture() { get_stream(conn).add_bytes(create_ok_frame(1, ok_builder().build())); }

    void validate_bytes_written()
    {
        BOOST_MYSQL_ASSERT_BUFFER_EQUALS(
            get_stream(conn).bytes_written(),
            create_query_frame(0, "SELECT 'abc'")
        );
    }

    struct request
    {
        string_view value;
        operator string_view() { return value; }  // Intentionally non-const
    };
    static request make_request() { return {"SELECT 'abc'"}; }
};

BOOST_FIXTURE_TEST_CASE(execute_sync_errc, fixture)
{
    // Setup
    error_code ec;
    diagnostics diag;

    // Run
    conn.execute(make_request(), result, ec, diag);

    // Validate
    BOOST_TEST(ec == error_code());
    BOOST_TEST(diag == diagnostics());
    validate_bytes_written();
}

BOOST_FIXTURE_TEST_CASE(execute_sync_exc, fixture)
{
    conn.execute(make_request(), result);
    validate_bytes_written();
}

BOOST_FIXTURE_TEST_CASE(execute_async_diag, fixture)
{
    diagnostics diag;
    conn.async_execute(make_request(), result, diag, as_netresult).validate_no_error();
    validate_bytes_written();
}

BOOST_FIXTURE_TEST_CASE(execute_async_nodiag, fixture)
{
    conn.async_execute(make_request(), result, as_netresult).validate_no_error();
    validate_bytes_written();
}

BOOST_FIXTURE_TEST_CASE(execute_async_deferred, fixture)
{
    // Spotcheck: deferred works correctly
    auto op = conn.async_execute(make_request(), result, asio::deferred);
    std::move(op)(as_netresult).validate_no_error();
    validate_bytes_written();
}

BOOST_FIXTURE_TEST_CASE(start_execution_sync_errc, fixture)
{
    // Setup
    error_code ec;
    diagnostics diag;

    // Run
    conn.start_execution(make_request(), st, ec, diag);

    // Validate
    BOOST_TEST(ec == error_code());
    BOOST_TEST(diag == diagnostics());
    validate_bytes_written();
}

BOOST_FIXTURE_TEST_CASE(start_execution_sync_exc, fixture)
{
    conn.start_execution(make_request(), st);
    validate_bytes_written();
}

BOOST_FIXTURE_TEST_CASE(start_execution_async_diag, fixture)
{
    diagnostics diag;
    conn.async_start_execution(make_request(), st, diag, as_netresult).validate_no_error();
    validate_bytes_written();
}

BOOST_FIXTURE_TEST_CASE(start_execution_async_nodiag, fixture)
{
    conn.async_start_execution(make_request(), st, as_netresult).validate_no_error();
    validate_bytes_written();
}

BOOST_FIXTURE_TEST_CASE(start_execution_async_deferred, fixture)
{
    // Spotcheck: deferred works correctly
    auto op = conn.async_start_execution(make_request(), st, asio::deferred);
    std::move(op)(as_netresult).validate_no_error();
    validate_bytes_written();
}

// Spotcheck: old connection doesn't generate compile errors.
// Intentionally not run
void old_connection()
{
    tcp_connection conn(global_context_executor());
    auto req = fixture::make_request();
    results result;
    execution_state st;
    error_code ec;
    diagnostics diag;

    conn.execute(req, result, ec, diag);
    conn.execute(req, result);
    conn.async_execute(req, result, asio::detached);
    conn.async_execute(req, result, diag, asio::detached);
    conn.async_execute(req, result, diag, asio::deferred)(asio::detached);
    conn.start_execution(req, st, ec, diag);
    conn.start_execution(req, st);
    conn.async_start_execution(req, st, asio::detached);
    conn.async_start_execution(req, st, diag, asio::detached);
    conn.async_start_execution(req, st, diag, asio::deferred)(asio::detached);
}

BOOST_AUTO_TEST_SUITE_END()

// The execution request is forwarded with the correct value category, and is moved if required.
// Move-only objects work correctly
BOOST_AUTO_TEST_SUITE(forwarding_rvalues)

struct fixture
{
    any_connection conn{create_test_any_connection()};
    results result;
    execution_state st;

    fixture() { get_stream(conn).add_bytes(create_ok_frame(1, ok_builder().build())); }

    void validate_bytes_written()
    {
        BOOST_MYSQL_ASSERT_BUFFER_EQUALS(
            get_stream(conn).bytes_written(),
            create_query_frame(0, "SELECT 'abcd'")
        );
    }

    // An execution request that doesn't support copies
    struct request
    {
        std::unique_ptr<char[]> buff;
        std::size_t size;

        request(string_view v) : buff(new char[v.size()]), size(v.size())
        {
            std::copy(v.begin(), v.end(), buff.get());
        }

        operator string_view() const { return {buff.get(), size}; }
    };
    static request make_request() { return {"SELECT 'abcd'"}; }
};

BOOST_FIXTURE_TEST_CASE(execute_sync_errc, fixture)
{
    // Setup
    error_code ec;
    diagnostics diag;

    // Execute
    conn.execute(make_request(), result, ec, diag);

    // Check
    BOOST_TEST(ec == error_code());
    BOOST_TEST(diag == diagnostics());
    validate_bytes_written();
}

BOOST_FIXTURE_TEST_CASE(execute_sync_exc, fixture)
{
    conn.execute(make_request(), result);
    validate_bytes_written();
}

BOOST_FIXTURE_TEST_CASE(execute_async_diag, fixture)
{
    diagnostics diag;
    conn.async_execute(make_request(), result, diag, as_netresult).validate_no_error();
    validate_bytes_written();
}

BOOST_FIXTURE_TEST_CASE(execute_async_nodiag, fixture)
{
    conn.async_execute(make_request(), result, as_netresult).validate_no_error();
    validate_bytes_written();
}

BOOST_FIXTURE_TEST_CASE(execute_async_deferred, fixture)
{
    auto op = conn.async_execute(make_request(), result, asio::deferred);
    std::move(op)(as_netresult).validate_no_error();
    validate_bytes_written();
}

BOOST_FIXTURE_TEST_CASE(start_execution_sync_errc, fixture)
{
    // Setup
    error_code ec;
    diagnostics diag;

    // Execute
    conn.start_execution(make_request(), st, ec, diag);

    // Check
    BOOST_TEST(ec == error_code());
    BOOST_TEST(diag == diagnostics());
    validate_bytes_written();
}

BOOST_FIXTURE_TEST_CASE(start_execution_sync_exc, fixture)
{
    conn.start_execution(make_request(), st);
    validate_bytes_written();
}

BOOST_FIXTURE_TEST_CASE(start_execution_async_diag, fixture)
{
    diagnostics diag;
    conn.async_start_execution(make_request(), st, diag, as_netresult).validate_no_error();
    validate_bytes_written();
}

BOOST_FIXTURE_TEST_CASE(start_execution_async_nodiag, fixture)
{
    conn.async_start_execution(make_request(), st, as_netresult).validate_no_error();
    validate_bytes_written();
}

BOOST_FIXTURE_TEST_CASE(start_execution_async_deferred, fixture)
{
    auto op = conn.async_start_execution(make_request(), st, asio::deferred);
    std::move(op)(as_netresult).validate_no_error();
    validate_bytes_written();
}
// Spotcheck: old connection doesn't generate compile errors.
// Intentionally not run
void old_connection()
{
    tcp_connection conn(global_context_executor());
    results result;
    execution_state st;
    error_code ec;
    diagnostics diag;

    conn.execute(fixture::make_request(), result, ec, diag);
    conn.execute(fixture::make_request(), result);
    conn.async_execute(fixture::make_request(), result, asio::detached);
    conn.async_execute(fixture::make_request(), result, diag, asio::detached);
    conn.async_execute(fixture::make_request(), result, diag, asio::deferred)(asio::detached);
    conn.start_execution(fixture::make_request(), st, ec, diag);
    conn.start_execution(fixture::make_request(), st);
    conn.async_start_execution(fixture::make_request(), st, asio::detached);
    conn.async_start_execution(fixture::make_request(), st, diag, asio::detached);
    conn.async_start_execution(fixture::make_request(), st, diag, asio::deferred)(asio::detached);
}

BOOST_AUTO_TEST_SUITE_END()

// The execution request is forwarded correctly, with the correct value category.
// Lvalues are not moved
BOOST_AUTO_TEST_SUITE(forwarding_lvalues)

struct fixture
{
    // A request where we can detect moved-from state.
    // std::string move-constructor doesn't offer guarantees about moved-from objects,
    // while std::vector does
    struct vector_request
    {
        std::vector<char> buff;

        vector_request(string_view v) : buff(v.begin(), v.end()) {}

        operator string_view() const { return {buff.data(), buff.size()}; }
    };

    any_connection conn{create_test_any_connection()};
    results result;
    execution_state st;
    vector_request req{"SELECT 'abcd'"};

    fixture() { get_stream(conn).add_bytes(create_ok_frame(1, ok_builder().build())); }

    void validate_bytes_written()
    {
        BOOST_MYSQL_ASSERT_BUFFER_EQUALS(
            get_stream(conn).bytes_written(),
            create_query_frame(0, "SELECT 'abcd'")
        );
        BOOST_TEST(static_cast<string_view>(req) == "SELECT 'abcd'");
    }
};

BOOST_FIXTURE_TEST_CASE(execute_sync_errc, fixture)
{
    // Setup
    error_code ec;
    diagnostics diag;

    // Execute
    conn.execute(req, result, ec, diag);

    // Validate
    BOOST_TEST(ec == error_code());
    BOOST_TEST(diag == diagnostics());
    validate_bytes_written();
}

BOOST_FIXTURE_TEST_CASE(execute_sync_exc, fixture)
{
    conn.execute(req, result);
    validate_bytes_written();
}

BOOST_FIXTURE_TEST_CASE(execute_async_diag, fixture)
{
    diagnostics diag;
    conn.async_execute(req, result, diag, as_netresult).validate_no_error();
    validate_bytes_written();
}

BOOST_FIXTURE_TEST_CASE(execute_async_nodiag, fixture)
{
    conn.async_execute(req, result, as_netresult).validate_no_error();
    validate_bytes_written();
}

BOOST_FIXTURE_TEST_CASE(execute_async_deferred, fixture)
{
    auto op = conn.async_execute(req, result, asio::deferred);
    std::move(op)(as_netresult).validate_no_error();
    validate_bytes_written();
}

BOOST_FIXTURE_TEST_CASE(start_execution_sync_errc, fixture)
{
    // Setup
    error_code ec;
    diagnostics diag;

    // Execute
    conn.start_execution(req, st, ec, diag);

    // Check
    BOOST_TEST(ec == error_code());
    BOOST_TEST(diag == diagnostics());
    validate_bytes_written();
}

BOOST_FIXTURE_TEST_CASE(start_execution_sync_exc, fixture)
{
    conn.start_execution(req, st);
    validate_bytes_written();
}

BOOST_FIXTURE_TEST_CASE(start_execution_async_diag, fixture)
{
    diagnostics diag;
    conn.async_start_execution(req, st, diag, as_netresult).validate_no_error();
    validate_bytes_written();
}

BOOST_FIXTURE_TEST_CASE(start_execution_async_nodiag, fixture)
{
    conn.async_start_execution(req, st, as_netresult).validate_no_error();
    validate_bytes_written();
}

BOOST_FIXTURE_TEST_CASE(start_execution_async_deferred, fixture)
{
    auto op = conn.async_start_execution(req, st, asio::deferred);
    std::move(op)(as_netresult).validate_no_error();
    validate_bytes_written();
}

// Spotcheck: old connection doesn't generate compile errors.
// Intentionally not run
void old_connection()
{
    tcp_connection conn(global_context_executor());
    results result;
    execution_state st;
    error_code ec;
    diagnostics diag;
    const fixture::vector_request req{"SELECT 'abc'"};

    conn.execute(req, result, ec, diag);
    conn.execute(req, result);
    conn.async_execute(req, result, asio::detached);
    conn.async_execute(req, result, diag, asio::detached);
    conn.async_execute(req, result, diag, asio::deferred)(asio::detached);
    conn.start_execution(req, st, ec, diag);
    conn.start_execution(req, st);
    conn.async_start_execution(req, st, asio::detached);
    conn.async_start_execution(req, st, diag, asio::detached);
    conn.async_start_execution(req, st, diag, asio::deferred)(asio::detached);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
