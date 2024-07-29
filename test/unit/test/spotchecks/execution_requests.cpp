//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Since integration tests can't reliably test multifunction operations
// that span over multiple messages, we test the complete multifn fllow in this unit tests.

#include <boost/mysql/connection.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/with_params.hpp>

#include <boost/test/unit_test.hpp>

#include <ranges>
#include <vector>

#include "test_common/assert_buffer_equals.hpp"
#include "test_common/has_ranges.hpp"
#include "test_unit/create_ok.hpp"
#include "test_unit/create_ok_frame.hpp"
#include "test_unit/create_query_frame.hpp"
#include "test_unit/test_stream.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;

BOOST_AUTO_TEST_SUITE(test_execution_requests)

// TODO: replace this by any_connection
using test_connection = connection<test_stream>;

//                 execute, start_execution 4 overloads, and with deferred/eager tokens
//                     const/nonconst correctly applied => with_params and a mutable range
//                     value category preserved => with_params and a move-only type

// ExecutionRequest is correctly forwarded by functions, and const-ness is preserved
#ifdef BOOST_MYSQL_HAS_RANGES
BOOST_AUTO_TEST_CASE(forwarding_constness)
{
    test_connection conn;
    results result;
    conn.stream().add_bytes(create_ok_frame(1, ok_builder().build()));

    std::vector<int> elms{1, 7, 4, 10};
    conn.execute(
        with_params("SELECT {}", elms | std::ranges::views::filter([](int i) { return i > 5; })),
        result
    );

    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(conn.stream().bytes_written(), create_query_frame(0, "SELECT 1, 4"));
}
#endif

BOOST_AUTO_TEST_SUITE_END()
