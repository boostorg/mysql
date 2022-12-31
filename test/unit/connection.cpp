//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/connection.hpp>
#include <boost/mysql/resultset.hpp>

#include <boost/test/unit_test.hpp>

#include "create_message.hpp"
#include "netfun_maker.hpp"
#include "test_connection.hpp"

using boost::mysql::resultset;
using namespace boost::mysql::test;

namespace {

using query_netfun_maker = netfun_maker_mem<void, test_connection, boost::string_view, resultset&>;

struct
{
    query_netfun_maker::signature query;
    const char* name;
} all_fns[] = {
    {query_netfun_maker::sync_errc(&test_connection::query),           "sync_errc"    },
    {query_netfun_maker::async_errinfo(&test_connection::async_query), "async_errinfo"},
};

BOOST_AUTO_TEST_SUITE(test_connection_)

// init ctor
BOOST_AUTO_TEST_CASE(init_ctor)
{
    test_connection c;
    BOOST_CHECK_NO_THROW(c.stream());
}

// move ctor
BOOST_AUTO_TEST_CASE(move_ctor)
{
    test_connection c1;
    test_connection c2{std::move(c1)};
    BOOST_CHECK_NO_THROW(c2.stream());
}

BOOST_AUTO_TEST_CASE(use_move_constructed_connection)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            // Construct connection
            test_connection conn;

            // Use it
            conn.stream().add_message(create_ok_packet_message_execute(1));
            resultset result;
            fns.query(conn, "SELECT * FROM myt", result).validate_no_error();

            // Move construct another connection from conn
            test_connection conn2(std::move(conn));

            // Using it works (no dangling pointer)
            conn2.stream().add_message(create_ok_packet_message_execute(1, 42));
            fns.query(conn2, "DELETE FROM myt", result).validate_no_error();
            BOOST_TEST(result.affected_rows() == 42u);
        }
    }
}

// move assign
BOOST_AUTO_TEST_CASE(move_assign_to_moved_from)
{
    test_connection moved_from;
    test_connection other{std::move(moved_from)};
    test_connection conn;
    moved_from = std::move(conn);
    BOOST_CHECK_NO_THROW(moved_from.stream());
}

BOOST_AUTO_TEST_CASE(move_assign_to_valid)
{
    test_connection c1;
    test_connection c2;
    c1 = std::move(c2);
    BOOST_CHECK_NO_THROW(c1.stream());
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
            conn1.stream().add_message(create_ok_packet_message_execute(1));
            conn2.stream().add_message(create_ok_packet_message_execute(1));
            resultset result;
            fns.query(conn1, "SELECT * FROM myt", result).validate_no_error();
            fns.query(conn2, "SELECT * FROM myt", result).validate_no_error();

            // Move assign
            conn2 = std::move(conn1);

            // Using it works (no dangling pointer)
            conn2.stream().add_message(create_ok_packet_message_execute(1, 42));
            fns.query(conn2, "DELETE FROM myt", result).validate_no_error();
            BOOST_TEST(result.affected_rows() == 42u);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()  // test_connection

}  // namespace
