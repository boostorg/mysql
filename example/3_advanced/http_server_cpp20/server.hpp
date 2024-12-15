//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
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

// TODO: review
// Launches a HTTP server that will listen on 0.0.0.0:port.
// If the server fails to launch (e.g. because the port is aleady in use),
// returns a non-zero error code. ex should identify the io_context or thread_pool
// where the server should run. The server is run until the underlying execution
// context is stopped.
boost::asio::awaitable<void> listener(boost::mysql::connection_pool& pool, unsigned short port);

}  // namespace orders

//]

#endif
