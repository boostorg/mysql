//
// Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "test_common.hpp"
#include "boost/mysql/prepared_statement.hpp"
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>

using namespace boost::mysql::detail;
using boost::mysql::prepared_statement;
using boost::mysql::tcp_prepared_statement;
using boost::asio::ip::tcp;

BOOST_AUTO_TEST_SUITE(test_prepared_statement)

struct prepared_statement_fixture
{
    boost::asio::io_context ctx;
    channel<tcp::socket> chan {ctx};
};

BOOST_FIXTURE_TEST_CASE(default_constructor, prepared_statement_fixture)
{
    tcp_prepared_statement stmt;
    BOOST_TEST(!stmt.valid());
}

BOOST_FIXTURE_TEST_CASE(initializing_constructor, prepared_statement_fixture)
{
    tcp_prepared_statement stmt (chan, com_stmt_prepare_ok_packet{10, 9, 8, 7});
    BOOST_TEST(stmt.valid());
    BOOST_TEST(stmt.id() == 10);
    BOOST_TEST(stmt.num_params() == 8);
}

BOOST_FIXTURE_TEST_CASE(move_constructor_from_default_constructed, prepared_statement_fixture)
{
    tcp_prepared_statement stmt {tcp_prepared_statement()};
    BOOST_TEST(!stmt.valid());
}

BOOST_FIXTURE_TEST_CASE(move_constructor_from_valid, prepared_statement_fixture)
{

    tcp_prepared_statement stmt (tcp_prepared_statement(
        chan, com_stmt_prepare_ok_packet{10, 9, 8, 7}
    ));
    BOOST_TEST(stmt.valid());
    BOOST_TEST(stmt.id() == 10);
    BOOST_TEST(stmt.num_params() == 8);
}

BOOST_FIXTURE_TEST_CASE(move_assignment_from_default_constructed, prepared_statement_fixture)
{
    tcp_prepared_statement stmt (
        chan,
        com_stmt_prepare_ok_packet{10, 9, 8, 7}
    );
    stmt = tcp_prepared_statement();
    BOOST_TEST(!stmt.valid());
    stmt = tcp_prepared_statement();
    BOOST_TEST(!stmt.valid());
}

BOOST_FIXTURE_TEST_CASE(move_assignment_from_valid, prepared_statement_fixture)
{

    tcp_prepared_statement stmt;
    stmt = tcp_prepared_statement (
        chan,
        com_stmt_prepare_ok_packet{10, 9, 8, 7}
    );
    BOOST_TEST(stmt.valid());
    BOOST_TEST(stmt.id() == 10);
    BOOST_TEST(stmt.num_params() == 8);
    stmt = tcp_prepared_statement(
        chan,
        com_stmt_prepare_ok_packet{1, 2, 3, 4}
    );
    BOOST_TEST(stmt.valid());
    BOOST_TEST(stmt.id() == 1);
    BOOST_TEST(stmt.num_params() == 3);
}

BOOST_AUTO_TEST_CASE(rebind_executor)
{
    using other_executor = boost::asio::strand<boost::asio::io_context::executor_type>;
    using rebound_type = tcp_prepared_statement::rebind_executor<other_executor>::other;
    using expected_type = boost::mysql::prepared_statement<
        boost::asio::basic_stream_socket<
            boost::asio::ip::tcp,
            other_executor
        >
    >;
    BOOST_TEST((std::is_same<rebound_type, expected_type>::value));
}

BOOST_AUTO_TEST_SUITE_END() // test_prepared_statement
