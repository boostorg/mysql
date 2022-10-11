//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_AUTH_MYSQL_NATIVE_PASSWORD_HPP
#define BOOST_MYSQL_DETAIL_AUTH_MYSQL_NATIVE_PASSWORD_HPP

#include <boost/mysql/detail/auxiliar/bytestring.hpp>
#include <boost/mysql/error.hpp>

#include <boost/utility/string_view.hpp>

#include <cstdint>

namespace boost {
namespace mysql {
namespace detail {
namespace mysql_native_password {

// Authorization for this plugin is always challenge (nonce) -> response
// (hashed password).
inline error_code compute_response(
    boost::string_view password,
    boost::string_view challenge,
    bool use_ssl,
    bytestring& output
);

}  // namespace mysql_native_password
}  // namespace detail
}  // namespace mysql
}  // namespace boost

#include <boost/mysql/detail/auth/impl/mysql_native_password.ipp>

#endif
