//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_ERROR_HPP
#define BOOST_MYSQL_ERROR_HPP

#include <boost/mysql/errc.hpp>

#include <boost/system/error_code.hpp>

#include <ostream>
#include <string>

namespace boost {
namespace mysql {

/// An alias for boost::system error codes.
using error_code = boost::system::error_code;

/// Creates an [reflink error_code] from an [reflink errc].
inline error_code make_error_code(errc error);

/**
 * \brief Additional information about error conditions
 * \details Contains an error message describing what happened. Not all error
 * conditions are able to generate this extended information - those that
 * can't have an empty error message.
 */
class error_info
{
    std::string msg_;

public:
    /// Default constructor.
    error_info() = default;

    /// Initialization constructor.
    error_info(std::string&& err) noexcept : msg_(std::move(err)) {}

    /// Gets the error message.
    const std::string& message() const noexcept { return msg_; }

    /// Sets the error message.
    void set_message(std::string&& err) { msg_ = std::move(err); }

    /// Restores the object to its initial state.
    void clear() noexcept { msg_.clear(); }
};

/**
 * \relates error_info
 * \brief Compare two error_info objects.
 */
inline bool operator==(const error_info& lhs, const error_info& rhs) noexcept
{
    return lhs.message() == rhs.message();
}

/**
 * \relates error_info
 * \brief Compare two error_info objects.
 */
inline bool operator!=(const error_info& lhs, const error_info& rhs) noexcept
{
    return !(lhs == rhs);
}

/**
 * \relates error_info
 * \brief Streams an error_info.
 */
inline std::ostream& operator<<(std::ostream& os, const error_info& v) { return os << v.message(); }

}  // namespace mysql
}  // namespace boost

#include <boost/mysql/impl/error.hpp>

#endif
