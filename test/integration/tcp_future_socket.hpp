//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_INTEGRATION_TCP_FUTURE_SOCKET_HPP
#define BOOST_MYSQL_TEST_INTEGRATION_TCP_FUTURE_SOCKET_HPP

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/use_future.hpp>

namespace boost {
namespace mysql {
namespace test {

// A TCP socket that has use_future as default completion token
class future_executor : public boost::asio::io_context::executor_type
{
public:
    future_executor(const boost::asio::io_context::executor_type& base) :
        boost::asio::io_context::executor_type(base) {}
    using default_completion_token_type = boost::asio::use_future_t<>;
};
using tcp_future_socket = boost::asio::basic_stream_socket<
    boost::asio::ip::tcp,
    future_executor
>;

} // test
} // mysql
} // test

#endif
