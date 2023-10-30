//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/buffer_params.hpp>
#include <boost/mysql/connection.hpp>
#include <boost/mysql/metadata_mode.hpp>
#include <boost/mysql/tcp.hpp>
#include <boost/mysql/tcp_ssl.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/test/unit_test.hpp>

#include "test_common/printing.hpp"

using namespace boost::mysql;
namespace net = boost::asio;

BOOST_AUTO_TEST_SUITE(test_connection_)

BOOST_AUTO_TEST_CASE(init_ctor)
{
    net::io_context ctx;
    tcp_connection c{ctx.get_executor()};
    BOOST_CHECK_NO_THROW(c.stream().get_executor());
}

BOOST_AUTO_TEST_CASE(init_ctor_with_buffer_params)
{
    net::io_context ctx;
    tcp_connection c{buffer_params(4096), ctx.get_executor()};
    BOOST_CHECK_NO_THROW(c.stream().get_executor());
}

BOOST_AUTO_TEST_CASE(init_ctor_with_buffer_params_rvalue)
{
    net::io_context ctx;
    tcp_connection::stream_type sock{ctx.get_executor()};
    tcp_connection c{buffer_params(4096), std::move(sock)};
    BOOST_CHECK_NO_THROW(c.stream().get_executor());
}

// move ctor
BOOST_AUTO_TEST_CASE(move_ctor)
{
    net::io_context ctx;
    tcp_connection c1{ctx.get_executor()};
    tcp_connection c2{std::move(c1)};
    BOOST_CHECK_NO_THROW(c2.stream().get_executor());
}

// move assign
BOOST_AUTO_TEST_CASE(move_assign_to_moved_from)
{
    net::io_context ctx;
    tcp_connection moved_from{ctx.get_executor()};
    tcp_connection other{std::move(moved_from)};
    tcp_connection conn{ctx.get_executor()};
    moved_from = std::move(conn);
    BOOST_CHECK_NO_THROW(moved_from.stream().get_executor());
}

BOOST_AUTO_TEST_CASE(move_assign_to_valid)
{
    net::io_context ctx;
    tcp_connection c1{ctx.get_executor()};
    tcp_connection c2{ctx.get_executor()};
    c1 = std::move(c2);
    BOOST_CHECK_NO_THROW(c1.stream());
}

BOOST_AUTO_TEST_CASE(set_meta_mode)
{
    net::io_context ctx;

    // Default metadata mode
    tcp_connection conn{ctx.get_executor()};
    BOOST_TEST(conn.meta_mode() == metadata_mode::minimal);

    // Setting it causes effect
    conn.set_meta_mode(metadata_mode::full);
    BOOST_TEST(conn.meta_mode() == metadata_mode::full);
}

// rebind_executor
using other_exec = net::strand<net::any_io_executor>;
static_assert(
    std::is_same<
        typename tcp_connection::rebind_executor<other_exec>::other,
        connection<net::basic_stream_socket<net::ip::tcp, other_exec>>>::value,
    ""
);
static_assert(
    std::is_same<
        typename tcp_ssl_connection::rebind_executor<other_exec>::other,
        connection<net::ssl::stream<net::basic_stream_socket<net::ip::tcp, other_exec>>>>::value,
    ""
);

// Verify that connection doesn't cause compilation errors for a minimal stream type
struct stream_archetype
{
    // Executor
    using executor_type = net::any_io_executor;
    executor_type get_executor() { return {}; }

    // Reading
    std::size_t read_some(net::mutable_buffer, error_code&) { return 0; }

    template <class CompletionToken>
    void async_read_some(net::mutable_buffer, CompletionToken&&)
    {
    }

    // Writing
    std::size_t write_some(net::const_buffer, error_code&) { return 0; }

    template <class CompletionToken>
    void async_write_some(net::const_buffer, CompletionToken&&)
    {
    }
};

connection<stream_archetype> conn;

BOOST_AUTO_TEST_SUITE_END()  // test_connection
