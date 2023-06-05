//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_AUXILIAR_ERROR_HELPERS_HPP
#define BOOST_MYSQL_DETAIL_AUXILIAR_ERROR_HELPERS_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/config.hpp>

#include <boost/assert/source_location.hpp>

namespace boost {
namespace mysql {
namespace detail {

BOOST_MYSQL_DECL
void throw_on_error_loc(error_code err, const diagnostics& diag, const boost::source_location& loc);

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
