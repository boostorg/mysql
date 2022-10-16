//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/statement_base.hpp>
#include <boost/mysql/tcp.hpp>

#include <boost/asio/ip/tcp.hpp>
#include <boost/test/tools/interface.hpp>

#include "integration_test_common.hpp"
#include "streams.hpp"
#include "tcp_network_fixture.hpp"

using namespace boost::mysql::test;
using boost::mysql::errc;
using boost::mysql::error_code;
using boost::mysql::tcp_resultset;
using boost::mysql::tcp_statement;

namespace {

auto net_samples = create_network_samples({
    "tcp_sync_errc",
    "tcp_async_callback",
});

BOOST_AUTO_TEST_SUITE(test_prepare_statement)

BOOST_MYSQL_NETWORK_TEST(success, network_fixture, all_network_samples())
{
    setup_and_connect(sample.net);
    conn->prepare_statement("SELECT * FROM empty_table WHERE id IN (?, ?)", *stmt)
        .validate_no_error();
    BOOST_TEST_REQUIRE(stmt->base().valid());
    BOOST_TEST(stmt->base().id() > 0u);
    BOOST_TEST(stmt->base().num_params() == 2u);
}

BOOST_MYSQL_NETWORK_TEST(error, network_fixture, all_network_samples())
{
    setup_and_connect(sample.net);
    conn->prepare_statement("SELECT * FROM bad_table WHERE id IN (?, ?)", *stmt)
        .validate_error(errc::no_such_table, {"table", "doesn't exist", "bad_table"});
}

BOOST_MYSQL_NETWORK_TEST(no_params, network_fixture, net_samples)
{
    setup_and_connect(sample.net);
    conn->prepare_statement("SELECT * FROM empty_table", *stmt).validate_no_error();
    BOOST_TEST_REQUIRE(stmt->base().valid());
    BOOST_TEST(stmt->base().id() > 0u);
    BOOST_TEST(stmt->base().num_params() == 0u);
}

BOOST_AUTO_TEST_SUITE_END()  // test_prepare_statement

}  // namespace
