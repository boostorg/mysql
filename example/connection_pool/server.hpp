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

namespace orders {

struct shared_state
{
    boost::mysql::connection_pool pool;

    shared_state(boost::mysql::connection_pool pool) : pool(std::move(pool)) {}
};

boost::system::error_code launch_server(
    boost::asio::io_context& ctx,
    unsigned short port,
    std::shared_ptr<shared_state> state
);

}  // namespace orders

#endif
