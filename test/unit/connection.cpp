//
// Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "boost/mysql/connection.hpp"
#include "test_stream.hpp"
#include <boost/asio/strand.hpp>
#include <boost/test/unit_test.hpp>

using conn_t = boost::mysql::connection<boost::mysql::test::test_stream>;

BOOST_AUTO_TEST_SUITE(test_connection)

// init ctor
BOOST_AUTO_TEST_CASE(init_ctor)
{
    conn_t c;
    BOOST_TEST(c.valid());
}

// move ctor
BOOST_AUTO_TEST_CASE(move_ctor_from_invalid)
{
    conn_t c1;
    conn_t c2 {std::move(c1)};
    conn_t c3 {std::move(c1)};
    BOOST_TEST(!c1.valid());
    BOOST_TEST(!c3.valid());
}

BOOST_AUTO_TEST_CASE(move_ctor_from_valid)
{
    conn_t c1;
    conn_t c2 {std::move(c1)};
    BOOST_TEST(!c1.valid());
    BOOST_TEST(c2.valid());
}

// move assign
BOOST_AUTO_TEST_CASE(move_assign_invalid_to_invalid)
{
    conn_t c1;
    conn_t c2 {std::move(c1)};
    conn_t c3;
    conn_t c4 {std::move(c2)};
    c3 = std::move(c1);
    BOOST_TEST(!c1.valid());
    BOOST_TEST(!c3.valid());
}

BOOST_AUTO_TEST_CASE(move_assign_invalid_to_valid)
{
    conn_t c1;
    conn_t c2 {std::move(c1)};
    conn_t c3;
    c1 = std::move(c3);
    BOOST_TEST(c1.valid());
    BOOST_TEST(!c3.valid());
}

BOOST_AUTO_TEST_CASE(move_assign_valid_to_invalid)
{
    conn_t c1;
    conn_t c2 {std::move(c1)};
    conn_t c3;
    c3 = std::move(c1);
    BOOST_TEST(!c1.valid());
    BOOST_TEST(!c3.valid());
}

BOOST_AUTO_TEST_CASE(move_assign_valid_to_valid)
{
    conn_t c1;
    conn_t c2;
    c1 = std::move(c2);
    BOOST_TEST(c1.valid());
    BOOST_TEST(!c2.valid());
}

using other_executor = boost::asio::strand<boost::asio::io_context::executor_type>;

BOOST_AUTO_TEST_CASE(connection_rebind_executor)
{
    using original_type = boost::mysql::connection<boost::asio::ip::tcp::socket>;
    using rebound_type = original_type::rebind_executor<other_executor>::other;
    using expected_type = boost::mysql::connection<
        boost::asio::basic_stream_socket<
            boost::asio::ip::tcp,
            other_executor
        >
    >;
    BOOST_TEST((std::is_same<rebound_type, expected_type>::value));
}

BOOST_AUTO_TEST_CASE(socket_connection_rebind_executor)
{
    using rebound_type = boost::mysql::tcp_connection::rebind_executor<other_executor>::other;
    using expected_type = boost::mysql::socket_connection<
        boost::asio::basic_stream_socket<
            boost::asio::ip::tcp,
            other_executor
        >
    >;
    BOOST_TEST((std::is_same<rebound_type, expected_type>::value));
}

BOOST_AUTO_TEST_SUITE_END() // test_connection
