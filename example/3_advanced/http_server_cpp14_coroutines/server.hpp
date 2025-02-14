//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_EXAMPLE_3_ADVANCED_HTTP_SERVER_CPP14_COROUTINES_SERVER_HPP
#define BOOST_MYSQL_EXAMPLE_3_ADVANCED_HTTP_SERVER_CPP14_COROUTINES_SERVER_HPP

//[example_http_server_cpp14_coroutines_server_hpp
//
// File: server.hpp
//

#include <boost/mysql/connection_pool.hpp>

#include <boost/asio/spawn.hpp>

#include <memory>

namespace notes {

// State shared by all sessions created by our server.
// For this application, we only need a connection_pool object.
// Place here any other singleton objects your application may need.
// We will use std::shared_ptr<shared_state> to ensure that objects
// are kept alive until all sessions are terminated.
struct shared_state
{
    boost::mysql::connection_pool pool;

    shared_state(boost::mysql::connection_pool pool) noexcept : pool(std::move(pool)) {}
};

// Runs a HTTP server that will listen on 0.0.0.0:port.
// If the server fails to launch (e.g. because the port is already in use),
// throws an exception. The server runs until the underlying execution
// context is stopped.
void run_server(std::shared_ptr<shared_state> st, unsigned short port, boost::asio::yield_context yield);

}  // namespace notes

//]

#endif
