//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/metadata_mode.hpp>
#include <boost/mysql/results.hpp>

#include <boost/asio/deferred.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/test/unit_test.hpp>

#include <stdexcept>
#include <string>

#include "test_common/printing.hpp"

using namespace boost::mysql;
namespace asio = boost::asio;
using asio::deferred;

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
    params.initial_buffer_size = 512u;
    any_connection c{ctx.get_executor(), params};
    BOOST_TEST((c.get_executor() == ctx.get_executor()));
}

BOOST_AUTO_TEST_CASE(init_ctor_max_buffer_size_eq_size)
{
    asio::io_context ctx;
    any_connection_params params;
    params.initial_buffer_size = 512u;
    params.max_buffer_size = 512u;
    any_connection c{ctx.get_executor(), params};
    BOOST_TEST((c.get_executor() == ctx.get_executor()));
}

BOOST_AUTO_TEST_CASE(init_ctor_error_max_buffer_size)
{
    asio::io_context ctx;
    any_connection_params params;
    params.initial_buffer_size = 513u;
    params.max_buffer_size = 512u;
    BOOST_CHECK_THROW(any_connection(ctx, params), std::invalid_argument);
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

// spotcheck: deferred compiles even in C++11
void deferred_spotcheck()
{
    asio::io_context ctx;
    any_connection conn(ctx);
    connect_params params;
    diagnostics diag;
    results result;
    execution_state st;
    statement stmt;
    std::string str;
    const std::string const_str;

    (void)conn.async_connect(params, deferred);
    (void)conn.async_connect(params, diag, deferred);

    (void)conn.async_execute("SELECT 1", result, deferred);
    (void)conn.async_execute(str, result, deferred);
    (void)conn.async_execute(const_str, result, deferred);
    (void)conn.async_execute("SELECT 1", result, diag, deferred);
    (void)conn.async_execute(str, result, diag, deferred);
    (void)conn.async_execute(const_str, result, diag, deferred);

    (void)conn.async_start_execution("SELECT 1", st, deferred);
    (void)conn.async_start_execution(str, st, deferred);
    (void)conn.async_start_execution(const_str, st, deferred);
    (void)conn.async_start_execution("SELECT 1", st, diag, deferred);
    (void)conn.async_start_execution(str, st, diag, deferred);
    (void)conn.async_start_execution(const_str, st, diag, deferred);

    (void)conn.async_read_some_rows(st, deferred);
    (void)conn.async_read_some_rows(st, diag, deferred);

    (void)conn.async_read_resultset_head(st, deferred);
    (void)conn.async_read_resultset_head(st, diag, deferred);

    (void)conn.async_prepare_statement("SELECT 1", deferred);
    (void)conn.async_prepare_statement("SELECT 1", diag, deferred);

    (void)conn.async_close_statement(stmt, deferred);
    (void)conn.async_close_statement(stmt, diag, deferred);

    (void)conn.async_reset_connection(deferred);
    (void)conn.async_reset_connection(diag, deferred);

    (void)conn.async_ping(deferred);
    (void)conn.async_ping(diag, deferred);

    (void)conn.async_close(deferred);
    (void)conn.async_close(diag, deferred);
}

BOOST_AUTO_TEST_SUITE_END()
