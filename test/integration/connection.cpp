//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "boost/mysql/connection.hpp"
#include "integration_test_common.hpp"
#include <gtest/gtest.h>

using namespace boost::mysql::test;
using boost::mysql::connection;
using boost::mysql::socket_connection;
using boost::asio::ip::tcp;

namespace
{

struct ConnectionTest : IntegTest<tcp::socket>
{
};

TEST_F(ConnectionTest, MoveConstructor_ConnectedConnection_UsableConnection)
{
    // First connection
    connection<tcp::socket> first (ctx);
    EXPECT_TRUE(first.valid());

    // Connect
    first.next_layer().connect(get_endpoint<tcp::socket>(endpoint_kind::localhost));
    first.handshake(connection_params);

    // Construct second connection
    connection<tcp::socket> second (std::move(first));
    EXPECT_FALSE(first.valid());
    EXPECT_TRUE(second.valid());
    EXPECT_NO_THROW(second.query("SELECT 1"));
}

TEST_F(ConnectionTest, MoveAssignment_FromConnectedConnection_UsableConnection)
{
    // First connection
    connection<tcp::socket> first (ctx);
    connection<tcp::socket> second (ctx);
    EXPECT_TRUE(first.valid());
    EXPECT_TRUE(second.valid());

    // Connect
    first.next_layer().connect(get_endpoint<tcp::socket>(endpoint_kind::localhost));
    first.handshake(connection_params);

    // Move
    second = std::move(first);
    EXPECT_FALSE(first.valid());
    EXPECT_TRUE(second.valid());
    EXPECT_NO_THROW(second.query("SELECT 1"));
}

struct SocketConnectionTest : IntegTest<tcp::socket>
{
};

TEST_F(SocketConnectionTest, MoveConstructor_ConnectedConnection_UsableConnection)
{
    // First connection
    socket_connection<tcp::socket> first (ctx);
    EXPECT_TRUE(first.valid());

    // Connect
    first.connect(get_endpoint<tcp::socket>(endpoint_kind::localhost), connection_params);

    // Construct second connection
    socket_connection<tcp::socket> second (std::move(first));
    EXPECT_FALSE(first.valid());
    EXPECT_TRUE(second.valid());
    EXPECT_NO_THROW(second.query("SELECT 1"));
}

TEST_F(SocketConnectionTest, MoveAssignment_FromConnectedConnection_UsableConnection)
{
    // First connection
    socket_connection<tcp::socket> first (ctx);
    socket_connection<tcp::socket> second (ctx);
    EXPECT_TRUE(first.valid());
    EXPECT_TRUE(second.valid());

    // Connect
    first.connect(get_endpoint<tcp::socket>(endpoint_kind::localhost), connection_params);

    // Move
    second = std::move(first);
    EXPECT_FALSE(first.valid());
    EXPECT_TRUE(second.valid());
    EXPECT_NO_THROW(second.query("SELECT 1"));
}

}
