//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_POOL_PARAMS_HPP
#define BOOST_MYSQL_POOL_PARAMS_HPP

#include <boost/mysql/any_address.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/ssl_mode.hpp>

#include <chrono>
#include <string>

namespace boost {
namespace mysql {

// TODO: review this
struct pool_params
{
    any_address address;
    std::string username;
    std::string password;
    std::string database;
    std::uint16_t connection_collation{handshake_params::default_collation};
    ssl_mode ssl{ssl_mode::require};
    bool multi_queries{};
    std::size_t initial_size{1};
    std::size_t max_size{150};
    std::chrono::steady_clock::duration connect_timeout{std::chrono::seconds(20)};
    std::chrono::steady_clock::duration ping_timeout{std::chrono::seconds(10)};
    std::chrono::steady_clock::duration reset_timeout{std::chrono::seconds(10)};
    std::chrono::steady_clock::duration retry_interval{std::chrono::seconds(30)};
    std::chrono::steady_clock::duration ping_interval{std::chrono::hours(1)};
    bool enable_thread_safety{true};
};

}  // namespace mysql
}  // namespace boost

#endif
