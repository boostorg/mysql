//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_CLIENT_ERRC_HPP
#define BOOST_MYSQL_CLIENT_ERRC_HPP

#include <boost/mysql/error_code.hpp>

#include <boost/system/error_category.hpp>

#include <ostream>

namespace boost {
namespace mysql {

/**
 * \brief MySQL client-defined error codes.
 * \details These errors are produced by the client itself, rather than the server.
 */
enum class client_errc : int
{
    /// An incomplete message was received from the server (indicates a deserialization error or
    /// packet mismatch).
    incomplete_message = 1,

    /// An unexpected value was found in a server-received message (indicates a deserialization
    /// error or packet mismatch).
    protocol_value_error,

    /// The server does not support the minimum required capabilities to establish the connection.
    server_unsupported,

    /// Unexpected extra bytes at the end of a message were received (indicates a deserialization
    /// error or packet mismatch).
    extra_bytes,

    /// Mismatched sequence numbers (usually caused by a packet mismatch).
    sequence_number_mismatch,

    /// The user employs an authentication plugin not known to this library.
    unknown_auth_plugin,

    /// The authentication plugin requires the connection to use SSL.
    auth_plugin_requires_ssl,

    /// The number of parameters passed to the prepared statement does not match the number of
    /// actual parameters.
    wrong_num_params
};

/**
 * \brief Returns the error_category associated to \ref client_errc.
 */
inline const boost::system::error_category& get_client_category() noexcept;

/// Creates an \ref error_code from a \ref client_errc.
inline error_code make_error_code(client_errc error);

/**
 * \brief Streams an error code.
 */
inline std::ostream& operator<<(std::ostream&, client_errc);

}  // namespace mysql
}  // namespace boost

#include <boost/mysql/impl/client_errc.hpp>

#endif
