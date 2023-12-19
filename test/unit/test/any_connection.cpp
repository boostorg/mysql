//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/metadata_mode.hpp>

#include <boost/test/unit_test.hpp>

#include "test_common/printing.hpp"

using namespace boost::mysql;
namespace asio = boost::asio;

BOOST_AUTO_TEST_SUITE(test_any_connection)

BOOST_AUTO_TEST_CASE(init_ctor)
{
    asio::io_context ctx;
    any_connection c{ctx.get_executor()};
    BOOST_TEST((c.get_executor() == ctx.get_executor()));
}

BOOST_AUTO_TEST_CASE(init_ctor_execution_context)
{
    asio::io_context ctx;
    any_connection c{ctx};
    BOOST_TEST((c.get_executor() == ctx.get_executor()));
}

BOOST_AUTO_TEST_CASE(init_ctor_with_buffer_size)
{
    asio::io_context ctx;
    any_connection_params params;
    params.initial_read_buffer_size = 512u;
    any_connection c{ctx.get_executor(), params};
    BOOST_TEST((c.get_executor() == ctx.get_executor()));
}

// move ctor
BOOST_AUTO_TEST_CASE(move_ctor)
{
    asio::io_context ctx;
    any_connection c1{ctx.get_executor()};
    any_connection c2{std::move(c1)};
    BOOST_TEST((c2.get_executor() == ctx.get_executor()));
}

// move assign
BOOST_AUTO_TEST_CASE(move_assign_to_moved_from)
{
    asio::io_context ctx;
    any_connection moved_from{ctx.get_executor()};
    any_connection other{std::move(moved_from)};
    any_connection conn{ctx.get_executor()};
    moved_from = std::move(conn);
    BOOST_TEST((moved_from.get_executor() == ctx.get_executor()));
}

BOOST_AUTO_TEST_CASE(move_assign_to_valid)
{
    asio::io_context ctx;
    any_connection c1{ctx.get_executor()};
    any_connection c2{ctx.get_executor()};
    c1 = std::move(c2);
    BOOST_TEST((c1.get_executor() == ctx.get_executor()));
}

BOOST_AUTO_TEST_CASE(set_meta_mode)
{
    asio::io_context ctx;

    // Default metadata mode
    any_connection conn{ctx.get_executor()};
    BOOST_TEST(conn.meta_mode() == metadata_mode::minimal);

    // Setting it causes effect
    conn.set_meta_mode(metadata_mode::full);
    BOOST_TEST(conn.meta_mode() == metadata_mode::full);
}

BOOST_AUTO_TEST_SUITE_END()
