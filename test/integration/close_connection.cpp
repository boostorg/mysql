//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "integration_test_common.hpp"

using namespace boost::mysql::test;
using boost::mysql::error_code;
using boost::mysql::socket_connection;

namespace
{

template <typename Stream>
struct CloseConnectionTest : public NetworkTest<Stream>
{
    void ActiveConnection_ClosesIt()
    {
        network_functions<Stream>* net = this->GetParam().net;

        // Close
        auto result = net->close(this->conn);
        result.validate_no_error();

        // We are no longer able to query
        auto query_result = net->query(this->conn, "SELECT 1");
        query_result.validate_any_error();

        // The stream is closed
        EXPECT_FALSE(this->conn.next_layer().is_open());
    }

    void DoubleClose_Ok()
    {
        network_functions<Stream>* net = this->GetParam().net;

        // Close
        auto result = net->close(this->conn);
        result.validate_no_error();

        // The stream (socket) is closed
        EXPECT_FALSE(this->conn.next_layer().is_open());

        // Closing again returns OK (and does nothing)
        result = net->close(this->conn);
        result.validate_no_error();

        // Stream (socket) still closed
        EXPECT_FALSE(this->conn.next_layer().is_open());
    }

    void CloseNotOpenedConnection_Ok()
    {
        network_functions<Stream>* net = this->GetParam().net;

        // An unconnected connection
        socket_connection<Stream> not_opened_conn (this->ctx);

        // Close
        auto result = net->close(not_opened_conn);
        result.validate_no_error();
    }
};

BOOST_MYSQL_NETWORK_TEST_SUITE(CloseConnectionTest)

BOOST_MYSQL_NETWORK_TEST(CloseConnectionTest, ActiveConnection_ClosesIt)
BOOST_MYSQL_NETWORK_TEST(CloseConnectionTest, DoubleClose_Ok)
BOOST_MYSQL_NETWORK_TEST(CloseConnectionTest, CloseNotOpenedConnection_Ok)

} // anon namespace


