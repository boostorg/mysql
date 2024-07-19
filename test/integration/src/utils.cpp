//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/defaults.hpp>
#include <boost/mysql/handshake_params.hpp>

#include <boost/assert/source_location.hpp>

#include "test_common/ci_server.hpp"
#include "test_common/network_result.hpp"
#include "test_integration/any_connection_fixture.hpp"
#include "test_integration/common.hpp"
#include "test_integration/tcp_connection_fixture.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;
namespace asio = boost::asio;

static any_connection_params make_params(asio::ssl::context& ssl_ctx)
{
    any_connection_params res;
    res.ssl_context = &ssl_ctx;
    return res;
}

// any_connection_fixture
any_connection_fixture::any_connection_fixture(any_connection_params params) : conn(ctx, params)
{
    conn.set_meta_mode(metadata_mode::full);
}

any_connection_fixture::any_connection_fixture(asio::ssl::context& ssl_ctx)
    : any_connection_fixture(make_params(ssl_ctx))
{
}

any_connection_fixture::~any_connection_fixture() { conn.async_close(as_netresult).validate_no_error(); }

void any_connection_fixture::connect(const connect_params& params, source_location loc)
{
    conn.async_connect(params, as_netresult).validate_no_error(loc);
}

void any_connection_fixture::connect(source_location loc)
{
    connect(connect_params_builder().ssl(ssl_mode::disable).build(), loc);
}

void any_connection_fixture::start_transaction(source_location loc)
{
    results r;
    conn.async_execute("START TRANSACTION", r, as_netresult).validate_no_error(loc);
}

// tcp_connection_fixture
static asio::ip::tcp::endpoint resolve_server_endpoint()
{
    asio::io_context ctx;
    asio::ip::tcp::resolver resolver(ctx);
    auto results = resolver.resolve(get_hostname(), default_port_string);
    return *results.begin();
}

tcp_connection_fixture::tcp_connection_fixture() : conn(ctx.get_executor())
{
    conn.set_meta_mode(metadata_mode::full);
}

tcp_connection_fixture::~tcp_connection_fixture() { conn.async_close(as_netresult).validate_no_error(); }

void tcp_connection_fixture::connect(source_location loc)
{
    connect(connect_params_builder().build_hparams(), loc);
}

void tcp_connection_fixture::connect(const handshake_params& params, source_location loc)
{
    conn.async_connect(get_tcp_endpoint(), params, as_netresult).validate_no_error(loc);
}

asio::ip::tcp::endpoint boost::mysql::test::get_tcp_endpoint()
{
    static auto res = resolve_server_endpoint();
    return res;
}