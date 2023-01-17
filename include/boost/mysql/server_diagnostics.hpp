//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_SERVER_DIAGNOSTICS_HPP
#define BOOST_MYSQL_SERVER_DIAGNOSTICS_HPP

#include <ostream>
#include <string>

namespace boost {
namespace mysql {

/**
 * \brief Additional information about server errors.
 * \details
 * Contains an error message describing what happened. The message may be empty,
 * if no diagnostic is available.
 *\n
 * \ref message is encoded according to `character_set_results` character set, which
 * usually matches the connection's character set. It's generated
 * by the server, and may potentially contain user input.
 */
class server_diagnostics
{
    std::string msg_;

public:
    /// Default constructor.
    server_diagnostics() = default;

    /// Initialization constructor.
    server_diagnostics(std::string&& err) noexcept : msg_(std::move(err)) {}

    /// Gets the error message.
    const std::string& message() const noexcept { return msg_; }

    /// Gets the error message.
    std::string& message() noexcept { return msg_; }

    /// Restores the object to its initial state.
    void clear() noexcept { msg_.clear(); }
};

/**
 * \relates server_diagnostics
 * \brief Compares two server_diagnostics objects.
 */
inline bool operator==(const server_diagnostics& lhs, const server_diagnostics& rhs) noexcept
{
    return lhs.message() == rhs.message();
}

/**
 * \relates server_diagnostics
 * \brief Compares two server_diagnostics objects.
 */
inline bool operator!=(const server_diagnostics& lhs, const server_diagnostics& rhs) noexcept
{
    return !(lhs == rhs);
}

}  // namespace mysql
}  // namespace boost

#endif
