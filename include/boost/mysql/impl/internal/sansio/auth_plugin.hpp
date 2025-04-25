//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_AUTH_PLUGIN_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_AUTH_PLUGIN_HPP

#include <boost/mysql/impl/internal/protocol/static_buffer.hpp>

namespace boost {
namespace mysql {
namespace detail {

// A buffer that can represent a password hashed used any of the authentication plugins
using hashed_password = static_buffer<32>;

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
