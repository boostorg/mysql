//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_EXAMPLE_3_ADVANCED_HTTP_SERVER_CPP20_SERVER_HPP
#define BOOST_MYSQL_EXAMPLE_3_ADVANCED_HTTP_SERVER_CPP20_SERVER_HPP

//[example_http_server_cpp20_server_hpp
//
// File: server.hpp
//

#include <boost/mysql/connection_pool.hpp>

#include <boost/asio/awaitable.hpp>

namespace orders {

// Launches a HTTP server that will listen on 0.0.0.0:port.
// If the server fails to launch (e.g. because the port is already in use),
// throws an exception. The server runs until the underlying execution
// context is stopped.
boost::asio::awaitable<void> run_server(boost::mysql::connection_pool& pool, unsigned short port);

}  // namespace orders

//]

#endif
