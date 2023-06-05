//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_SRC_ERROR_SERVER_ERROR_TO_STRING_HPP
#define BOOST_MYSQL_SRC_ERROR_SERVER_ERROR_TO_STRING_HPP

namespace boost {
namespace mysql {
namespace detail {

// Returns NULL if this is not a common error (not a member of common_server_errc)
const char* common_error_to_string(int v) noexcept;

// These return a default string if the error is not known
const char* mysql_error_to_string(int v) noexcept;
const char* mariadb_error_to_string(int v) noexcept;

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
