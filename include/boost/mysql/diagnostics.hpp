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
 * failed. It can contain server-generated messages (\ref server_message) or client-side messages
 * (\ref client_message). More members may be added in the future.
 */
class diagnostics
{
public:
    /**
     * \brief Constructs a diagnostics object with empty error messages.
     * \par Exception safety
     * No-throw guarantee.
     */
    diagnostics() = default;

    /**
     * \brief Gets the client-generated error message.
     * \details
     * Contrary to \ref server_message, the client message never contains any string data
     * returned by the server, and is always ASCII-encoded. If you're using the static interface,
     * it may contain C++ type identifiers, too.
     *
     * \par Exception safety
     * No-throw guarantee.
     *
     * \par Object lifetimes
     * The returned view is valid as long as `*this` is alive, hasn't been assigned-to
     * or moved-from, and \ref clear hasn't been called. Moving `*this` invalidates the view.
     */
    string_view client_message() const noexcept { return is_server_ ? string_view() : string_view(msg_); }

    /**
     * \brief Gets the server-generated error message.
     * \details
     * It's encoded according to `character_set_results` character set, which
     * usually matches the connection's character set. It may potentially contain user input.
     *
     * \par Exception safety
     * No-throw guarantee.
     *
     * \par Object lifetimes
     * The returned view is valid as long as `*this` is alive, hasn't been assigned-to
     * or moved-from, and \ref clear hasn't been called. Moving `*this` invalidates the view.
     */
    string_view server_message() const noexcept { return is_server_ ? string_view(msg_) : string_view(); }

    /**
     * \brief Clears the error messages.
     * \par Exception safety
     * No-throw guarantee.
     */
    void clear() noexcept
    {
        is_server_ = false;
        msg_.clear();
    }

private:
    bool is_server_{};
    std::string msg_;

#ifndef BOOST_MYSQL_DOXYGEN
    friend bool operator==(const diagnostics& lhs, const diagnostics& rhs) noexcept;
    friend struct detail::diagnostics_access;
#endif
};

/**
 * \relates diagnostics
 * \brief Compares two diagnostics objects.
 * \par Exception safety
 * No-throw guarantee.
 */
inline bool operator==(const diagnostics& lhs, const diagnostics& rhs) noexcept
{
    return lhs.is_server_ == rhs.is_server_ && lhs.msg_ == rhs.msg_;
}

/**
 * \relates diagnostics
 * \brief Compares two diagnostics objects.
 * \par Exception safety
 * No-throw guarantee.
 */
inline bool operator!=(const diagnostics& lhs, const diagnostics& rhs) noexcept { return !(lhs == rhs); }

}  // namespace mysql
}  // namespace boost

#include <boost/mysql/impl/diagnostics.hpp>

#endif
