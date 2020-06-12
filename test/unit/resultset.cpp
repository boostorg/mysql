//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "boost/mysql/resultset.hpp"
#include <boost/asio/strand.hpp>
#include <gtest/gtest.h>

namespace
{

TEST(ResultsetTest, RebindExecutor_Trivial_ReturnsCorrectType)
{
    using other_executor = boost::asio::strand<boost::asio::io_context::executor_type>;
    using rebound_type = boost::mysql::tcp_resultset::rebind_executor<other_executor>::other;
    using expected_type = boost::mysql::resultset<
        boost::asio::basic_stream_socket<
            boost::asio::ip::tcp,
            other_executor
        >
    >;
    EXPECT_TRUE((std::is_same<rebound_type, expected_type>::value));
}

}

