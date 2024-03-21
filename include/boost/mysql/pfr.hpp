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

// TODO: document

template <class T>
struct pfr_by_position;

#if BOOST_PFR_CORE_NAME_ENABLED

template <class T>
struct pfr_by_name;

#endif

}  // namespace mysql
}  // namespace boost

#include <boost/mysql/impl/pfr.hpp>

#endif
