//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_INTEGRATION_INCLUDE_TEST_INTEGRATION_ANY_CONNECTION_FIXTURE_HPP
#define BOOST_MYSQL_TEST_INTEGRATION_INCLUDE_TEST_INTEGRATION_ANY_CONNECTION_FIXTURE_HPP

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/metadata_mode.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/ssl_mode.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/assert/source_location.hpp>

#include "test_common/as_netres.hpp"
#include "test_integration/common.hpp"

namespace boost {
namespace mysql {
namespace test {

// TODO: move to compiled?
struct any_connection_fixture
{
    asio::io_context ctx;
    any_connection conn{ctx};

    static any_connection_params make_params(asio::ssl::context& ssl_ctx)
    {
        any_connection_params res;
        res.ssl_context = &ssl_ctx;
        return res;
    }

    any_connection_fixture() { conn.set_meta_mode(metadata_mode::full); }
    any_connection_fixture(any_connection_params params) : conn(ctx, params)
    {
        conn.set_meta_mode(metadata_mode::full);
    }
    any_connection_fixture(asio::ssl::context& ssl_ctx) : any_connection_fixture(make_params(ssl_ctx)) {}

    void connect(const connect_params& params, source_location loc = BOOST_CURRENT_LOCATION)
    {
        conn.async_connect(params, as_netresult).validate_no_error(loc);
    }

    void connect(source_location loc = BOOST_CURRENT_LOCATION)
    {
        connect(connect_params_builder().ssl(ssl_mode::disable).build(), loc);
    }

    void start_transaction(source_location loc = BOOST_CURRENT_LOCATION)
    {
        results r;
        conn.async_execute("START TRANSACTION", r, as_netresult).validate_no_error(loc);
    }
};

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
