//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_SERVER_ERROR_HPP
#define BOOST_MYSQL_SERVER_ERROR_HPP

#include <boost/mysql/error_code.hpp>
#include <boost/mysql/server_diagnostics.hpp>

#include <boost/system/system_error.hpp>

namespace boost {
namespace mysql {

/**
 * \brief Exception thrown to indicate an error originated in the server.
 * \details
 * Like `boost::system::system_error`, but adds a \ref diagnostics member
 * containing additional error-related information.
 */
class server_error : public boost::system::system_error
{
    server_diagnostics diag_;

public:
    /// Initializing constructor.
    server_error(const error_code& err, const server_diagnostics& diag)
        : boost::system::system_error(err), diag_(diag)
    {
    }

    /**
     * \brief Retrieves the server diagnostics embedded in this object.
     */
    const server_diagnostics& diagnostics() const noexcept { return diag_; }
};

}  // namespace mysql
}  // namespace boost

#endif
