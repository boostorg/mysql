//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_CONNECT_PARAMS_HPP
#define BOOST_MYSQL_CONNECT_PARAMS_HPP

#include <boost/mysql/any_address.hpp>
#include <boost/mysql/ssl_mode.hpp>
#include <boost/mysql/string_view.hpp>

#include <string>

namespace boost {
namespace mysql {

/**
 * \brief (EXPERIMENTAL) Parameters to be used with \ref any_connection functions.
 * \details
 * To be passed to \ref any_connection::connect and \ref any_connection::async_connect.
 * Includes the server address and MySQL handshake parameters.
 * \n
 * Contrary to \ref handshake_params, this is an owning type.
 */
struct connect_params
{
    /**
     * \brief Determines how to establish a physical connection to the MySQL server.
     * \details
     * This can be either a host and port or a UNIX socket path.
     * Defaults to (localhost, 3306).
     */
    any_address server_address;

    /// User name to authenticate as.
    std::string username;

    /// Password for that username, possibly empty.
    std::string password;

    /// Database name to use, or empty string for no database (this is the default).
    std::string database;

    /**
     * \brief Controls whether to use TLS or not.
     * \details
     * See \ref ssl_mode for more information about the possible modes.
     * This option is only relevant when `server_address.type() == address_type::host_and_port`.
     * UNIX socket connections will never use TLS, regardless of this value.
     */
    ssl_mode ssl{ssl_mode::require};

    /**
     * \brief Whether to enable support for executing semicolon-separated text queries.
     * \details Disabled by default.
     */
    bool multi_queries{false};
};

}  // namespace mysql
}  // namespace boost

#endif
