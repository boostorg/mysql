//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "integration_test_common.hpp"

using namespace boost::mysql::test;
using boost::mysql::socket_connection;
using boost::mysql::errc;
using boost::mysql::error_code;

namespace
{

template <typename Stream>
struct ConnectTest : NetworkTest<Stream>
{
    network_functions<Stream>* net;
    socket_connection<Stream> other_conn;

    ConnectTest():
        net(this->GetParam().net),
        other_conn(this->ctx)
    {
    }

    void AllSucceeded_SuccessfulLogin()
    {
        auto ep = get_endpoint<Stream>(endpoint_kind::localhost);
        auto result = net->connect(other_conn, ep, this->connection_params);
        result.validate_no_error();

        // We are able to query
        auto query_result = net->query(other_conn, "SELECT 1");
        query_result.validate_no_error();
        query_result.value.fetch_all(); // discard any result
    }

    void PhysicalConnectFailed_FailsClosesStream()
    {
        auto ep = get_endpoint<Stream>(endpoint_kind::inexistent);
        auto result = net->connect(other_conn, ep, this->connection_params);
        // depending on system and stream type, error code will be different
        result.validate_any_error({"physical connect failed"});
        EXPECT_FALSE(other_conn.next_layer().is_open());
    }

    void PhysicalConnectSucceededHandshakeFailed_FailsClosesStream()
    {
        auto ep = get_endpoint<Stream>(endpoint_kind::localhost);
        this->set_credentials("integ_user", "bad_password");
        auto result = net->connect(other_conn, ep, this->connection_params);
        result.validate_error(errc::access_denied_error, {"access denied", "integ_user"});
        EXPECT_FALSE(other_conn.next_layer().is_open());
    }

};

BOOST_MYSQL_NETWORK_TEST_SUITE(ConnectTest)

BOOST_MYSQL_NETWORK_TEST(ConnectTest, AllSucceeded_SuccessfulLogin)
BOOST_MYSQL_NETWORK_TEST(ConnectTest, PhysicalConnectFailed_FailsClosesStream)
BOOST_MYSQL_NETWORK_TEST(ConnectTest, PhysicalConnectSucceededHandshakeFailed_FailsClosesStream)

}

