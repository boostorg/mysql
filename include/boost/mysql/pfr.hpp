//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_PFR_HPP
#define BOOST_MYSQL_PFR_HPP

#include <boost/pfr/core_name.hpp>

namespace boost {
namespace mysql {

#if BOOST_PFR_CORE_NAME_ENABLED

// TODO: document
template <class T>
struct pfr_by_name;

#endif

}  // namespace mysql
}  // namespace boost

#if BOOST_PFR_CORE_NAME_ENABLED
#include <boost/mysql/impl/pfr_by_name.hpp>
#endif

#endif
