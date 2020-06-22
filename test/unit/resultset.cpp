//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "boost/mysql/resultset.hpp"
#include <boost/asio/strand.hpp>
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(test_resultset)

BOOST_AUTO_TEST_CASE(rebind_executor)
{
    using other_executor = boost::asio::strand<boost::asio::io_context::executor_type>;
    using rebound_type = boost::mysql::tcp_resultset::rebind_executor<other_executor>::other;
    using expected_type = boost::mysql::resultset<
        boost::asio::basic_stream_socket<
            boost::asio::ip::tcp,
            other_executor
        >
    >;
    BOOST_TEST((std::is_same<rebound_type, expected_type>::value));
}

BOOST_AUTO_TEST_SUITE_END() // test_resultset
