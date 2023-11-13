//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_POOL_PARAMS_HPP
#define BOOST_MYSQL_POOL_PARAMS_HPP

#include <boost/mysql/any_address.hpp>
#include <boost/mysql/buffer_params.hpp>
#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/ssl_mode.hpp>

#include <boost/asio/ssl/context.hpp>

#include <chrono>
#include <cstddef>
#include <string>

namespace boost {
namespace mysql {

struct pool_params
{
    any_address server_address{host_and_port("localhost")};
    std::string username;
    std::string password;
    std::string database;
    ssl_mode ssl{ssl_mode::require};
    bool multi_queries{};
    std::size_t initial_read_buffer_size{buffer_params::default_initial_read_size};
    std::size_t initial_size{1};
    std::size_t max_size{151};
    bool enable_thread_safety{true};
    asio::ssl::context* ssl_ctx{};
    std::chrono::steady_clock::duration connect_timeout{std::chrono::seconds(20)};
    std::chrono::steady_clock::duration ping_timeout{std::chrono::seconds(10)};
    std::chrono::steady_clock::duration retry_interval{std::chrono::seconds(30)};
    std::chrono::steady_clock::duration ping_interval{std::chrono::hours(1)};
};

}  // namespace mysql
}  // namespace boost

#endif
