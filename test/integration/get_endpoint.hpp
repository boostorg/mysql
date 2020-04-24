//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_INTEGRATION_GET_ENDPOINT_HPP
#define BOOST_MYSQL_TEST_INTEGRATION_GET_ENDPOINT_HPP

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <stdexcept>

namespace boost {
namespace mysql {
namespace test {

enum class endpoint_kind
{
    localhost,
    inexistent
};

template <typename Stream>
typename Stream::endpoint_type get_endpoint(endpoint_kind);

template <>
inline boost::asio::ip::tcp::endpoint
get_endpoint<boost::asio::ip::tcp::socket>(
    endpoint_kind kind
)
{
    if (kind == endpoint_kind::localhost)
    {
        return boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::loopback(), 3306);
    }
    else if (kind == endpoint_kind::inexistent)
    {
        return boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::loopback(), 45678);
    }
    else
    {
        throw std::invalid_argument("endpoint_kind");
    }
}

#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
template <>
inline boost::asio::local::stream_protocol::endpoint
get_endpoint<boost::asio::local::stream_protocol::socket>(
    endpoint_kind kind
)
{
    if (kind == endpoint_kind::localhost)
    {
        return boost::asio::local::stream_protocol::endpoint("/var/run/mysqld/mysqld.sock");
    }
    else if (kind == endpoint_kind::inexistent)
    {
        return boost::asio::local::stream_protocol::endpoint("/tmp/this/endpoint/does/not/exist");
    }
    else
    {
        throw std::invalid_argument("endpoint_kind");
    }
}
#endif

}
}
}



#endif
