//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_CLOSE_CONNECTION_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_CLOSE_CONNECTION_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/config.hpp>

#include <boost/asio/any_completion_handler.hpp>

namespace boost {
namespace mysql {
namespace detail {

class channel;

BOOST_MYSQL_DECL
void close_connection_impl(channel& chan, error_code& code, diagnostics& diag);

BOOST_MYSQL_DECL void async_close_connection_impl(
    channel& chan,
    diagnostics& diag,
    asio::any_completion_handler<void(error_code)> handler
);

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#ifdef BOOST_MYSQL_SOURCE
#include <boost/mysql/detail/network_algorithms/impl/close_connection.hpp>
#endif

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_CLOSE_CONNECTION_HPP_ */
