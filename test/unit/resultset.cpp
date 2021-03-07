//
// Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "boost/mysql/resultset.hpp"
#include "boost/mysql/detail/protocol/channel.hpp"
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/test/unit_test.hpp>
#include "test_stream.hpp"

using resultset_t = boost::mysql::resultset<boost::mysql::test::test_stream>;
using chan_t = boost::mysql::detail::channel<boost::mysql::test::test_stream>; 

BOOST_AUTO_TEST_SUITE(test_resultset)

// default ctor
BOOST_AUTO_TEST_CASE(default_ctor)
{
    resultset_t r;
    BOOST_TEST(!r.valid());
}

// move ctor
BOOST_AUTO_TEST_CASE(move_ctor_from_invalid)
{
    resultset_t r1;
    resultset_t r2 (std::move(r1));
    BOOST_TEST(!r1.valid());
    BOOST_TEST(!r2.valid());
}

BOOST_AUTO_TEST_CASE(move_ctor_from_valid)
{
    chan_t chan;
    resultset_t r1 (chan, {}, {});
    resultset_t r2 (std::move(r1));
    BOOST_TEST(!r1.valid());
    BOOST_TEST(r2.valid());
}

// move assignment
BOOST_AUTO_TEST_CASE(move_assign_invalid_to_invalid)
{
    resultset_t r1;
    resultset_t r2;
    r2 = std::move(r1);
    BOOST_TEST(!r1.valid());
    BOOST_TEST(!r2.valid());
}

BOOST_AUTO_TEST_CASE(move_assign_invalid_to_valid)
{
    chan_t chan;
    resultset_t r1;
    resultset_t r2 (chan, {}, {});
    r2 = std::move(r1);
    BOOST_TEST(!r1.valid());
    BOOST_TEST(!r2.valid());
}

BOOST_AUTO_TEST_CASE(move_assign_valid_to_invalid)
{
    chan_t chan;
    resultset_t r1 (chan, {}, {});
    resultset_t r2;
    r2 = std::move(r1);
    BOOST_TEST(!r1.valid());
    BOOST_TEST(r2.valid());
}

BOOST_AUTO_TEST_CASE(move_assign_valid_to_valid)
{
    chan_t chan;
    resultset_t r1 (chan, {}, {});
    resultset_t r2 (chan, {}, {});
    r2 = std::move(r1);
    BOOST_TEST(!r1.valid());
    BOOST_TEST(r2.valid());
}

// rebind executor
BOOST_AUTO_TEST_CASE(rebind_executor)
{
    using other_executor = boost::asio::strand<boost::asio::io_context::executor_type>;
    using rebound_type = boost::mysql::tcp_resultset::rebind_executor<other_executor>::other;
    using expected_type = boost::mysql::resultset<
        boost::asio::basic_stream_socket<
            boost::asio::ip::tcp,
            other_executor
        >
    >;
    BOOST_TEST((std::is_same<rebound_type, expected_type>::value));
}

BOOST_AUTO_TEST_SUITE_END() // test_resultset
