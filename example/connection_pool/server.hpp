//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_EXAMPLE_ORDER_MANAGEMENT_HTTP_SERVER_HPP
#define BOOST_MYSQL_EXAMPLE_ORDER_MANAGEMENT_HTTP_SERVER_HPP

#include <boost/mysql/connection_pool.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/system/error_code.hpp>

#include <memory>

#include "repository.hpp"

namespace notes {

// State shared by all sessions created by our server.
// For this application, we only need a connection_pool object.
// Place here any other singleton objects your application may need.
// We will use std::shared_ptr<shared_state> to ensure that objects
// are kept alive until all sessions are terminated.
struct shared_state
{
    boost::mysql::connection_pool pool;

    shared_state(boost::mysql::connection_pool pool) : pool(std::move(pool)) {}
};

// Launches a HTTP server that will listen on 0.0.0.0:4000.
// If the server fails to launch (e.g. because the port is aleady in use),
// returns a non-zero error code. The server runs in the background
// until ctx is stopped
boost::system::error_code launch_server(boost::asio::io_context& ctx, std::shared_ptr<shared_state> state);

}  // namespace notes

#endif
