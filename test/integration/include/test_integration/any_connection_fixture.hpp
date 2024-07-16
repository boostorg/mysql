//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_INTEGRATION_INCLUDE_TEST_INTEGRATION_ANY_CONNECTION_FIXTURE_HPP
#define BOOST_MYSQL_TEST_INTEGRATION_INCLUDE_TEST_INTEGRATION_ANY_CONNECTION_FIXTURE_HPP

#include <boost/mysql/any_connection.hpp>

#include <boost/asio/io_context.hpp>

namespace boost {
namespace mysql {
namespace test {

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

    any_connection_fixture() = default;
    any_connection_fixture(any_connection_params params) : conn(ctx, params) {}
    any_connection_fixture(asio::ssl::context& ssl_ctx) : any_connection_fixture(make_params(ssl_ctx)) {}
};

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
