//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_CONNECTION_PARAMS_HPP
#define BOOST_MYSQL_CONNECTION_PARAMS_HPP

#include <boost/utility/string_view.hpp>
#include "boost/mysql/collation.hpp"

/**
 * \defgroup connparams Connection parameters
 * \ingroup connection
 * \brief Parameters for estabilishing the connection to the MySQL server.
 */

namespace boost {
namespace mysql {

/**
 * \ingroup connparams
 * \brief Determines whether to use TLS for the connection or not.
 */
enum class ssl_mode
{
    disable, ///< Never use TLS
    enable,  ///< Use TLS if the server supports it, fall back to non-encrypted connection if it does not.
    require  ///< Always use TLS; abort the connection if the server does not support it.
};

/**
 * \ingroup connparams
 * \brief Connection options regarding TLS.
 * \details At the moment, contains only the ssl_mode, which
 * indicates whether to use TLS on the connection or not.
 */
class ssl_options
{
    ssl_mode mode_;
public:
    /**
     * \brief Default and initialization constructor.
     * \details By default, SSL is enabled for the connection
     * if the server supports is (ssl_mode::enable).
     */
    explicit ssl_options(ssl_mode mode=ssl_mode::enable) noexcept:
        mode_(mode) {}

    /// Retrieves the TLS mode to be used for the connection.
    ssl_mode mode() const noexcept { return mode_; }
};


/**
 * \ingroup connparams
 * \brief Parameters defining how to authenticate to a MySQL server.
 */
class connection_params
{
    boost::string_view username_;
    boost::string_view password_;
    boost::string_view database_;
    collation connection_collation_;
    ssl_options ssl_;
public:
    /// Initializing constructor
    connection_params(
        boost::string_view username,  ///< Username to authenticate as
        boost::string_view password,  ///< Password for that username, possibly empty.
        boost::string_view db = "",   ///< Database to use, or empty string for no database.
        collation connection_col = collation::utf8_general_ci, ///< The default character set and collation for the connection.
        const ssl_options& opts = ssl_options() ///< The TLS options to use with this connection.
    ) :
        username_(username),
        password_(password),
        database_(db),
        connection_collation_(connection_col),
        ssl_(opts)
    {
    }

    /// Retrieves the username.
    boost::string_view username() const noexcept { return username_; }

    /// Sets the username.
    void set_username(boost::string_view value) noexcept { username_ = value; }

    /// Retrieves the password.
    boost::string_view password() const noexcept { return password_; }

    /// Sets the password
    void set_password(boost::string_view value) noexcept { password_ = value; }

    /// Retrieves the database.
    boost::string_view database() const noexcept { return database_; }

    /// Sets the database
    void set_database(boost::string_view value) noexcept { database_ = value; }

    /// Retrieves the connection collation.
    collation connection_collation() const noexcept { return connection_collation_; }

    /// Sets the connection collation
    void set_connection_collation(collation value) noexcept { connection_collation_ = value; }

    /// Retrieves SSL options
    const ssl_options& ssl() const noexcept { return ssl_; }

    /// Sets SSL options
    void set_ssl(const ssl_options& value) noexcept { ssl_ = value; }
};

} // mysql
} // boost



#endif /* INCLUDE_BOOST_MYSQL_CONNECTION_PARAMS_HPP_ */
