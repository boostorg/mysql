//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/connection.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/test/unit_test.hpp>

#include <cstdlib>

#include "get_endpoint.hpp"

namespace {

// Get the endpoint to use for TCP from an environment variable.
// Required as CI MySQL doesn't run on loocalhost
struct global_fixture
{
    static boost::asio::ip::tcp::endpoint valid_endpoint;

    global_fixture()
    {
        const char* hostname = std::getenv("BOOST_MYSQL_SERVER_HOST");
        if (hostname)
        {
            boost::asio::io_context ctx;
            boost::asio::ip::tcp::resolver resolver(ctx.get_executor());
            auto results = resolver.resolve(hostname, boost::mysql::default_port_string);
            valid_endpoint = *results.begin();
        }
    }
};

boost::asio::ip::tcp::endpoint global_fixture::valid_endpoint(
    boost::asio::ip::address_v4::loopback(),
    boost::mysql::default_port
);

BOOST_TEST_GLOBAL_FIXTURE(global_fixture);

}  // namespace

boost::asio::ip::tcp::endpoint boost::mysql::test::endpoint_getter<
    boost::asio::ip::tcp>::operator()(er_endpoint kind)
{
    if (kind == er_endpoint::valid)
    {
        return global_fixture::valid_endpoint;
    }
    else if (kind == er_endpoint::inexistent)
    {
        return boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::loopback(), 45678);
    }
    else
    {
        throw std::invalid_argument("er_endpoint");
    }
}

#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
boost::asio::local::stream_protocol::endpoint boost::mysql::test::endpoint_getter<
    boost::asio::local::stream_protocol>::operator()(er_endpoint kind)
{
    if (kind == er_endpoint::valid)
    {
        return boost::asio::local::stream_protocol::endpoint("/var/run/mysqld/mysqld.sock");
    }
    else if (kind == er_endpoint::inexistent)
    {
        return boost::asio::local::stream_protocol::endpoint("/tmp/this/endpoint/does/not/exist");
    }
    else
    {
        throw std::invalid_argument("er_endpoint");
    }
}
#endif
