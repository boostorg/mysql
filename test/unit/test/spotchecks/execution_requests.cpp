//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Since integration tests can't reliably test multifunction operations
// that span over multiple messages, we test the complete multifn fllow in this unit tests.

#include <boost/mysql/character_set.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/string_view.hpp>
#include <boost/mysql/with_params.hpp>

#include <boost/mysql/detail/access.hpp>

#include <boost/asio/deferred.hpp>
#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "test_common/assert_buffer_equals.hpp"
#include "test_common/network_result.hpp"
#include "test_unit/create_ok.hpp"
#include "test_unit/create_ok_frame.hpp"
#include "test_unit/create_query_frame.hpp"
#include "test_unit/test_any_connection.hpp"
#include "test_unit/test_stream.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;
namespace asio = boost::asio;

BOOST_AUTO_TEST_SUITE(test_execution_requests)

// TODO: do we want to spotcheck old connection, too?

//                 execute, start_execution 4 overloads, and with deferred/eager tokens
//                     const/nonconst correctly applied => with_params and a mutable range
//                     value category preserved => with_params and a move-only type

// ExecutionRequest is correctly forwarded by functions, and const-ness is preserved
// netmakers intentionally not used here

// A request for which make_request requires a non-const object
// This happens with mutable ranges and with_params (e.g. with filter)
struct nonconst_request
{
    string_view value;

    operator string_view() { return value; }
};

BOOST_AUTO_TEST_CASE(forwarding_constness)
{
    auto conn = create_test_any_connection();
    results result;
    error_code ec;
    diagnostics diag;
    auto& stream = get_stream(conn);

    // any_connection, execute, sync_errc
    stream.add_bytes(create_ok_frame(1, ok_builder().build()));
    conn.execute(nonconst_request{"SELECT 'abc'"}, result, ec, diag);
    BOOST_TEST(ec == error_code());
    BOOST_TEST(diag == diagnostics());
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(get_stream(conn).bytes_written(), create_query_frame(0, "SELECT 'abc'"));

    // // any_connection, execute, sync_exc (TODO)

    // // deferred tokens
    // stream.clear();
    // stream.add_bytes(create_ok_frame(1, ok_builder().build()));
    // conn.async_execute(with_params("SELECT {}", elms | std::ranges::views::filter(pred)), result,
    // asio::deferred)(
    //         as_netresult
    // )
    //     .validate_no_error();
    // BOOST_MYSQL_ASSERT_BUFFER_EQUALS(get_stream(conn).bytes_written(), create_query_frame(0, "SELECT 1,
    // 4"));
}

struct move_only_request
{
    std::unique_ptr<char[]> buff;
    std::size_t size;

    move_only_request(string_view v) : buff(new char[v.size()]), size(v.size())
    {
        std::copy(v.begin(), v.end(), buff.get());
    }

    operator string_view() const { return {buff.get(), size}; }
};

BOOST_AUTO_TEST_CASE(forwarding_rvalues)
{
    auto conn = create_test_any_connection();
    results result;
    error_code ec;
    diagnostics diag;
    auto& stream = get_stream(conn);

    // any_connection, execute, sync_errc
    move_only_request req{"SELECT 'abcd'"};
    stream.add_bytes(create_ok_frame(1, ok_builder().build()));
    conn.execute(std::move(req), result, ec, diag);
    BOOST_TEST(ec == error_code());
    BOOST_TEST(diag == diagnostics());
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(
        get_stream(conn).bytes_written(),
        create_query_frame(0, "SELECT 'abcd'")
    );
}

// std::string move-constructor doesn't offer guarantees about moved-from objects,
// while std::vector does
struct vector_request
{
    std::vector<char> buff;

    vector_request(string_view v) : buff(v.begin(), v.end()) {}

    operator string_view() const { return {buff.data(), buff.size()}; }
};

BOOST_AUTO_TEST_CASE(forwarding_lvalues)
{
    auto conn = create_test_any_connection();
    results result;
    error_code ec;
    diagnostics diag;
    auto& stream = get_stream(conn);

    // any_connection, execute, sync_errc
    vector_request req{"SELECT 'abcd'"};
    stream.add_bytes(create_ok_frame(1, ok_builder().build()));
    conn.execute(req, result, ec, diag);
    BOOST_TEST(ec == error_code());
    BOOST_TEST(diag == diagnostics());
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(
        get_stream(conn).bytes_written(),
        create_query_frame(0, "SELECT 'abcd'")
    );
    BOOST_TEST(static_cast<string_view>(req) == "SELECT 'abcd'");  // original value unmodified
}

BOOST_AUTO_TEST_CASE(forwarding_const_lvalues)
{
    auto conn = create_test_any_connection();
    results result;
    error_code ec;
    diagnostics diag;
    auto& stream = get_stream(conn);

    // any_connection, execute, sync_errc
    const vector_request req{"SELECT 'abcd'"};
    stream.add_bytes(create_ok_frame(1, ok_builder().build()));
    conn.execute(req, result, ec, diag);
    BOOST_TEST(ec == error_code());
    BOOST_TEST(diag == diagnostics());
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(
        get_stream(conn).bytes_written(),
        create_query_frame(0, "SELECT 'abcd'")
    );
    BOOST_TEST(static_cast<string_view>(req) == "SELECT 'abcd'");  // original value unmodified
}

BOOST_AUTO_TEST_SUITE_END()
