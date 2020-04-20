//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_AUTH_MYSQL_NATIVE_PASSWORD_HPP
#define BOOST_MYSQL_DETAIL_AUTH_MYSQL_NATIVE_PASSWORD_HPP

#include <cstdint>
#include <string_view>
#include "boost/mysql/error.hpp"

namespace boost {
namespace mysql {
namespace detail {
namespace mysql_native_password {

// Authorization for this plugin is always challenge (nonce) -> response
// (hashed password).
inline error_code compute_response(
	std::string_view password,
	std::string_view challenge,
	bool use_ssl,
	std::string& output
);


} // mysql_native_password
} // detail
} // mysql
} // boost

#include "boost/mysql/detail/auth/impl/mysql_native_password.ipp"

#endif
