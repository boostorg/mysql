//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TCP_HPP
#define BOOST_MYSQL_TCP_HPP

#include <boost/mysql/connection.hpp>

#include <boost/asio/ip/tcp.hpp>

namespace boost {
namespace mysql {

/// A connection to MySQL over a TCP socket.
using tcp_connection = connection<boost::asio::ip::tcp::socket>;

/// The statement type to use with \ref tcp_connection.
using tcp_statement = typename tcp_connection::statement_type;

/// The resultset type to use with \ref tcp_connection.
using tcp_resultset = typename tcp_connection::resultset_type;

}  // namespace mysql
}  // namespace boost

#endif