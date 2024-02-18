//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/connection.hpp>
#include <boost/mysql/results.hpp>

#include <boost/test/unit_test.hpp>

#include "test_common/netfun_maker.hpp"
#include "test_common/printing.hpp"
#include "test_unit/create_ok.hpp"
#include "test_unit/create_ok_frame.hpp"
#include "test_unit/test_stream.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;

BOOST_AUTO_TEST_SUITE(test_connection_use_after_move)

using test_connection = connection<test_stream>;

using query_netfun_maker = netfun_maker_mem<void, test_connection, const string_view&, results&>;

struct
{
    query_netfun_maker::signature query;
    const char* name;
} all_fns[] = {
    {query_netfun_maker::sync_errc(&test_connection::execute),           "sync" },
    {query_netfun_maker::async_errinfo(&test_connection::async_execute), "async"},
};

BOOST_AUTO_TEST_CASE(use_move_constructed_connection)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            // Construct connection
            test_connection conn;

            // Use it
            conn.stream().add_bytes(create_ok_frame(1, ok_builder().build()));
            results result;
            fns.query(conn, "SELECT * FROM myt", result).validate_no_error();

            // Move construct another connection from conn
            test_connection conn2(std::move(conn));

            // Using it works (no dangling pointer)
            conn2.stream().add_bytes(create_ok_frame(1, ok_builder().affected_rows(42).build()));
            fns.query(conn2, "DELETE FROM myt", result).validate_no_error();
            BOOST_TEST(result.affected_rows() == 42u);
        }
    }
}

BOOST_AUTO_TEST_CASE(use_move_assigned_connection)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            // Construct two connections
            test_connection conn1;
            test_connection conn2;

            // Use them
            conn1.stream().add_bytes(create_ok_frame(1, ok_builder().build()));
            conn2.stream().add_bytes(create_ok_frame(1, ok_builder().build()));
            results result;
            fns.query(conn1, "SELECT * FROM myt", result).validate_no_error();
            fns.query(conn2, "SELECT * FROM myt", result).validate_no_error();

            // Move assign
            conn2 = std::move(conn1);

            // Using it works (no dangling pointer)
            conn2.stream().add_bytes(create_ok_frame(1, ok_builder().affected_rows(42).build()));
            fns.query(conn2, "DELETE FROM myt", result).validate_no_error();
            BOOST_TEST(result.affected_rows() == 42u);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
