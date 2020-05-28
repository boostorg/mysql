//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "boost/mysql/connection.hpp"
#include <boost/asio/strand.hpp>
#include <gtest/gtest.h>

namespace
{

using other_executor = boost::asio::strand<boost::asio::io_context::executor_type>;

TEST(ConnectionTest, RebindExecutor_Trivial_ReturnsCorrectType)
{
    using original_type = boost::mysql::connection<boost::asio::ip::tcp::socket>;
    using rebound_type = original_type::rebind_executor<other_executor>::other;
    using expected_type = boost::mysql::connection<
        boost::asio::basic_stream_socket<
            boost::asio::ip::tcp,
            other_executor
        >
    >;
    EXPECT_TRUE((std::is_same_v<rebound_type, expected_type>));
}

TEST(SocketConnectionTest, RebindExecutor_Trivial_ReturnsCorrectType)
{
    using rebound_type = boost::mysql::tcp_connection::rebind_executor<other_executor>::other;
    using expected_type = boost::mysql::socket_connection<
        boost::asio::basic_stream_socket<
            boost::asio::ip::tcp,
            other_executor
        >
    >;
    EXPECT_TRUE((std::is_same_v<rebound_type, expected_type>));
}

}

