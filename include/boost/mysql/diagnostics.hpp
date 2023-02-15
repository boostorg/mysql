//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DIAGNOSTICS_HPP
#define BOOST_MYSQL_DIAGNOSTICS_HPP

#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/auxiliar/access_fwd.hpp>

#include <ostream>
#include <string>

namespace boost {
namespace mysql {

/**
 * \brief Contains additional information about errors.
 * \details
 * This class is a container for additional diagnostics about an operation that
 * failed. Right now, it's used to hold any error messages sent by the server on
 * error (\ref server_message). More members may be added in the future.
 * The message may be empty, if no diagnostic is available.
 *\n
 * \ref server_message is encoded according to `character_set_results` character set, which
 * usually matches the connection's character set. It may potentially contain user input.
 */
class diagnostics
{
public:
    /**
     * \brief Constructs an empty diagnostics object.
     */
    diagnostics() = default;

    /// Gets the server-generated error message.
    string_view server_message() const noexcept { return msg_; }

    /// Restores the object to its initial state.
    void clear() noexcept { msg_.clear(); }

private:
    std::string msg_;

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::diagnostics_access;
#endif
};

/**
 * \relates diagnostics
 * \brief Compares two diagnostics objects.
 */
inline bool operator==(const diagnostics& lhs, const diagnostics& rhs) noexcept
{
    return lhs.server_message() == rhs.server_message();
}

/**
 * \relates diagnostics
 * \brief Compares two diagnostics objects.
 */
inline bool operator!=(const diagnostics& lhs, const diagnostics& rhs) noexcept { return !(lhs == rhs); }

}  // namespace mysql
}  // namespace boost

#include <boost/mysql/impl/diagnostics.hpp>

#endif
