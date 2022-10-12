//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_INTEGRATION_UTILS_INCLUDE_ER_NETWORK_VARIANT_HPP
#define BOOST_MYSQL_TEST_INTEGRATION_UTILS_INCLUDE_ER_NETWORK_VARIANT_HPP

#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/core/span.hpp>

#include <functional>

#include "er_connection.hpp"

namespace boost {
namespace mysql {
namespace test {

class er_network_variant
{
public:
    virtual ~er_network_variant() {}
    virtual bool supports_ssl() const = 0;
    virtual const char* stream_name() const = 0;
    virtual const char* variant_name() const = 0;
    virtual er_connection_ptr create(boost::asio::io_context::executor_type, boost::asio::ssl::context&) = 0;
};

boost::span<er_network_variant*> all_variants();
boost::span<er_network_variant*> ssl_variants();
boost::span<er_network_variant*> non_ssl_variants();
er_network_variant* tcp_sync_errc_variant();

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
