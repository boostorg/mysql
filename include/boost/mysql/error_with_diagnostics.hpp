//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_ERROR_WITH_DIAGNOSTICS_HPP
#define BOOST_MYSQL_ERROR_WITH_DIAGNOSTICS_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/config.hpp>

#include <boost/system/system_error.hpp>

namespace boost {
namespace mysql {

/**
 * \brief A system_error with an embedded diagnostics object.
 * \details
 * Like `boost::system::system_error`, but adds a \ref diagnostics member
 * containing additional information.
 */
class error_with_diagnostics : public system::system_error
{
    diagnostics diag_;

    static system::system_error create_base(const error_code& err, const diagnostics& diag)
    {
        return diag.client_message().empty() ? system::system_error(err)
                                             : system::system_error(err, diag.client_message());
    }

public:
    /// Initializing constructor.
    error_with_diagnostics(const error_code& err, const diagnostics& diag)
        : system::system_error(create_base(err, diag)), diag_(diag)
    {
    }

    /**
     * \brief Retrieves the server diagnostics embedded in this object.
     * \par Exception safety
     * No-throw guarantee.
     *
     * \par Object lifetimes
     * The returned reference is valid as long as `*this` is alive.
     */
    const diagnostics& get_diagnostics() const noexcept { return diag_; }
};

/**
 * \brief A custom error type to be used with system::result.
 * \details
 * A custom error type containing an error code and a diagnostics object.
 * It can be used as the second template parameter of `boost::system::result`
 * (`boost::system::result<T, errcode_with_diagnostics>`).
 * \n
 * When `system::result::value()` throws, the exception type is \ref error_with_diagnostics,
 * containing the error code and diagnostics contained in this type.
 */
struct errcode_with_diagnostics
{
    /// The error code.
    error_code code;

    /// The diagnostics object.
    diagnostics diag;
};

// Required to make errcode_and_diagnostics compatible with boost::system
#ifndef BOOST_MYSQL_DOXYGEN
[[noreturn]] BOOST_MYSQL_DECL void throw_exception_from_error(
    const errcode_with_diagnostics& e,
    const source_location& loc
);
#endif

}  // namespace mysql
}  // namespace boost

#ifdef BOOST_MYSQL_HEADER_ONLY
#include <boost/mysql/impl/error_with_diagnostics.ipp>
#endif

#endif
