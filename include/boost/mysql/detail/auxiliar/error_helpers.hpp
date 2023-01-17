//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_AUXILIAR_ERROR_HELPERS_HPP
#define BOOST_MYSQL_DETAIL_AUXILIAR_ERROR_HELPERS_HPP

#include <boost/mysql/error_code.hpp>
#include <boost/mysql/server_diagnostics.hpp>
#include <boost/mysql/server_errc.hpp>
#include <boost/mysql/server_error.hpp>

#include <boost/throw_exception.hpp>

namespace boost {
namespace mysql {
namespace detail {

inline void throw_on_error_loc(
    error_code err,
    const server_diagnostics& diag,
    const boost::source_location& loc

)
{
    if (err)
    {
        if (err.category() == get_server_category())
        {
            ::boost::throw_exception(server_error(err, diag), loc);
        }
        else
        {
            ::boost::throw_exception(boost::system::system_error(err), loc);
        }
    }
}

inline void clear_errors(error_code& err, server_diagnostics& diag) noexcept
{
    err.clear();
    diag.clear();
}

// Helper to implement sync with exceptions functions
struct error_block
{
    error_code err;
    server_diagnostics diag;

    void check(const boost::source_location& loc) { throw_on_error_loc(err, diag, loc); }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
