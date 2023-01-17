//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_THROW_ON_ERROR_HPP
#define BOOST_MYSQL_THROW_ON_ERROR_HPP

#include <boost/mysql/error_code.hpp>
#include <boost/mysql/server_diagnostics.hpp>

#include <boost/mysql/detail/auxiliar/error_helpers.hpp>

namespace boost {
namespace mysql {

/**
 * \brief Throws an exception in case of error, possibly including server diagnostics.
 * \details If err indicates a failure (`err.failed() == true`), throws an exception that
 * derives from `boost::system::system_error`. If `err.category()` corresponds to a MySQL
 * server category error, the exception will additionally derive from \ref server_error,
 * and will make `diag` available in \ref server_error::diagnostics.
 */
inline void throw_on_error(error_code err, const server_diagnostics& diag = {})
{
    detail::throw_on_error_loc(err, diag, boost::source_location{});
}

}  // namespace mysql
}  // namespace boost

#endif
