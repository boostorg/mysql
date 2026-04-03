//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/defaults.hpp>
#include <boost/mysql/handshake_params.hpp>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/assert/source_location.hpp>

#include <exception>

#include "test_common/ci_server.hpp"
// #include "test_integration/any_connection_fixture.hpp"
#include "test_integration/connect_params_builder.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;

// //
// // any_connection_fixture.hpp
// //
// static any_connection_params make_params(asio::ssl::context& ssl_ctx)
// {
//     any_connection_params res;
//     res.ssl_context = &ssl_ctx;
//     return res;
// }

// any_connection_fixture::any_connection_fixture(any_connection_params params) : conn(ctx, params)
// {
//     conn.set_meta_mode(metadata_mode::full);
// }

// any_connection_fixture::any_connection_fixture(asio::ssl::context& ssl_ctx)
//     : any_connection_fixture(make_params(ssl_ctx))
// {
// }

// any_connection_fixture::~any_connection_fixture() { conn.async_close(as_netresult).validate_no_error(); }

// void any_connection_fixture::connect(const connect_params& params, boost::source_location loc)
// {
//     conn.async_connect(params, as_netresult).validate_no_error(loc);
// }

// void any_connection_fixture::connect(boost::source_location loc)
// {
//     connect(connect_params_builder().ssl(ssl_mode::disable).build(), loc);
// }

// void any_connection_fixture::start_transaction(boost::source_location loc)
// {
//     results r;
//     conn.async_execute("START TRANSACTION", r, as_netresult).validate_no_error(loc);
// }

//
// connect_params_builder.hpp
//
connect_params connect_params_builder::build()
{
    connect_params res;
    res.server_address = std::move(addr_);
    res.username = res_.username();
    res.password = res_.password();
    res.database = res_.database();
    res.multi_queries = res_.multi_queries();
    res.ssl = res_.ssl();
    res.connection_collation = res_.connection_collation();
    return res;
}
