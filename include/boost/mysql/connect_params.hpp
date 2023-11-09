//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_CONNECT_PARAMS_HPP
#define BOOST_MYSQL_CONNECT_PARAMS_HPP

#include <boost/mysql/any_address.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/ssl_mode.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/access.hpp>

#include <string>

namespace boost {
namespace mysql {

struct connect_params
{
    any_address server_address{any_address::make_tcp("localhost")};
    std::string username;
    std::string password;
    std::string database;
    std::uint16_t connection_collation{handshake_params::default_collation};
    ssl_mode ssl{ssl_mode::require};
    bool multi_queries{};
};

}  // namespace mysql
}  // namespace boost

#endif
