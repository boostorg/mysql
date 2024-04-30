//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IS_FATAL_ERROR_HPP
#define BOOST_MYSQL_IS_FATAL_ERROR_HPP

#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/config.hpp>

namespace boost {
namespace mysql {

BOOST_MYSQL_DECL
bool is_fatal_error(error_code ec) noexcept;

}  // namespace mysql
}  // namespace boost

#ifdef BOOST_MYSQL_HEADER_ONLY
#include <boost/mysql/impl/is_fatal_error.ipp>
#endif

#endif
