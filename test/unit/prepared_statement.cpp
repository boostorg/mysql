//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "test_common.hpp"
#include "boost/mysql/prepared_statement.hpp"
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>

using namespace boost::mysql::detail;
using boost::mysql::prepared_statement;
using boost::mysql::tcp_prepared_statement;
using boost::asio::ip::tcp;

namespace
{

struct PreparedStatementTest : public testing::Test
{
    boost::asio::io_context ctx; // TODO: change these for a mock stream
    channel<tcp::socket> chan {ctx};
};

TEST_F(PreparedStatementTest, DefaultConstructor_Trivial_Invalid)
{
    tcp_prepared_statement stmt;
    EXPECT_FALSE(stmt.valid());
}

TEST_F(PreparedStatementTest, InitializingConstructor_Trivial_Valid)
{
    tcp_prepared_statement stmt (chan, com_stmt_prepare_ok_packet{10, 9, 8, 7});
    EXPECT_TRUE(stmt.valid());
    EXPECT_EQ(stmt.id(), 10);
    EXPECT_EQ(stmt.num_params(), 8);
}

TEST_F(PreparedStatementTest, MoveConstructor_FromDefaultConstructed_Invalid)
{
    tcp_prepared_statement stmt {tcp_prepared_statement()};
    EXPECT_FALSE(stmt.valid());
}

TEST_F(PreparedStatementTest, MoveConstructor_FromValid_Valid)
{

    tcp_prepared_statement stmt (tcp_prepared_statement(
        chan, com_stmt_prepare_ok_packet{10, 9, 8, 7}
    ));
    EXPECT_TRUE(stmt.valid());
    EXPECT_EQ(stmt.id(), 10);
    EXPECT_EQ(stmt.num_params(), 8);
}

TEST_F(PreparedStatementTest, MoveAssignment_FromDefaultConstructed_Invalid)
{
    tcp_prepared_statement stmt (
        chan,
        com_stmt_prepare_ok_packet{10, 9, 8, 7}
    );
    stmt = tcp_prepared_statement();
    EXPECT_FALSE(stmt.valid());
    stmt = tcp_prepared_statement();
    EXPECT_FALSE(stmt.valid());
}

TEST_F(PreparedStatementTest, MoveAssignment_FromValid_Valid)
{

    tcp_prepared_statement stmt;
    stmt = tcp_prepared_statement (
        chan,
        com_stmt_prepare_ok_packet{10, 9, 8, 7}
    );
    EXPECT_TRUE(stmt.valid());
    EXPECT_EQ(stmt.id(), 10);
    EXPECT_EQ(stmt.num_params(), 8);
    stmt = tcp_prepared_statement(
        chan,
        com_stmt_prepare_ok_packet{1, 2, 3, 4}
    );
    EXPECT_TRUE(stmt.valid());
    EXPECT_EQ(stmt.id(), 1);
    EXPECT_EQ(stmt.num_params(), 3);
}

TEST_F(PreparedStatementTest, RebindExecutor_Trivial_ReturnsCorrectType)
{
    using other_executor = boost::asio::strand<boost::asio::io_context::executor_type>;
    using rebound_type = tcp_prepared_statement::rebind_executor<other_executor>::other;
    using expected_type = boost::mysql::prepared_statement<
        boost::asio::basic_stream_socket<
            boost::asio::ip::tcp,
            other_executor
        >
    >;
    EXPECT_TRUE((std::is_same<rebound_type, expected_type>::value));
}

}


