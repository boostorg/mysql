//
// Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "boost/mysql/detail/protocol/channel.hpp"
#include "test_common.hpp"
#include "test_stream.hpp"
#include "boost/mysql/prepared_statement.hpp"
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/test/unit_test_suite.hpp>

using namespace boost::mysql::detail;
using boost::mysql::prepared_statement;

using chan_t = boost::mysql::detail::channel<boost::mysql::test::test_stream>;
using stmt_t = prepared_statement<boost::mysql::test::test_stream>;

BOOST_AUTO_TEST_SUITE(test_prepared_statement)

// default ctor
BOOST_AUTO_TEST_CASE(default_ctor)
{
    stmt_t s;
    BOOST_TEST(!s.valid());
}

// init ctor
BOOST_AUTO_TEST_CASE(init_ctor)
{
    chan_t chan;
    stmt_t s (chan, com_stmt_prepare_ok_packet{10, 9, 8, 7});
    BOOST_TEST(s.valid());
    BOOST_TEST(s.id() == 10);
    BOOST_TEST(s.num_params() == 8);
}

// move ctor
BOOST_AUTO_TEST_CASE(move_ctor_from_invalid)
{
    stmt_t s1;
    stmt_t s2 {std::move(s1)};
    BOOST_TEST(!s1.valid());
    BOOST_TEST(!s2.valid());
}

BOOST_AUTO_TEST_CASE(move_ctor_from_valid)
{
    chan_t chan;
    stmt_t s1 (chan, com_stmt_prepare_ok_packet{10, 9, 8, 7});
    stmt_t s2 (std::move(s1));
    BOOST_TEST(!s1.valid());
    BOOST_TEST(s2.valid());
    BOOST_TEST(s2.id() == 10);
    BOOST_TEST(s2.num_params() == 8);
}

// move assign
BOOST_AUTO_TEST_CASE(move_assign_invalid_to_invalid)
{
    stmt_t s1;
    stmt_t s2;
    s2 = std::move(s1);
    BOOST_TEST(!s1.valid());
    BOOST_TEST(!s2.valid());
}

BOOST_AUTO_TEST_CASE(move_assign_invalid_to_valid)
{
    chan_t chan;
    stmt_t s1;
    stmt_t s2 (chan, com_stmt_prepare_ok_packet{10, 9, 8, 7});
    s2 = std::move(s1);
    BOOST_TEST(!s1.valid());
    BOOST_TEST(!s2.valid());
}

BOOST_AUTO_TEST_CASE(move_assign_valid_to_invalid)
{
    chan_t chan;
    stmt_t s1 (chan, com_stmt_prepare_ok_packet{10, 9, 8, 7});
    stmt_t s2;
    s2 = std::move(s1);
    BOOST_TEST(!s1.valid());
    BOOST_TEST(s2.valid());
    BOOST_TEST(s2.id() == 10);
    BOOST_TEST(s2.num_params() == 8);
}

BOOST_AUTO_TEST_CASE(move_assign_valid_to_valid)
{
    chan_t chan;
    stmt_t s1 (chan, com_stmt_prepare_ok_packet{10, 9, 8, 7});
    stmt_t s2 (chan, com_stmt_prepare_ok_packet{1, 2, 3, 4});
    s2 = std::move(s1);
    BOOST_TEST(!s1.valid());
    BOOST_TEST(s2.valid());
    BOOST_TEST(s2.id() == 10);
    BOOST_TEST(s2.num_params() == 8);
}

// rebind executor
BOOST_AUTO_TEST_CASE(rebind_executor)
{
    using other_executor = boost::asio::strand<boost::asio::io_context::executor_type>;
    using rebound_type = boost::mysql::tcp_prepared_statement::rebind_executor<other_executor>::other;
    using expected_type = boost::mysql::prepared_statement<
        boost::asio::basic_stream_socket<
            boost::asio::ip::tcp,
            other_executor
        >
    >;
    BOOST_TEST((std::is_same<rebound_type, expected_type>::value));
}

BOOST_AUTO_TEST_SUITE_END() // test_prepared_statement
