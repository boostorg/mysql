//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_OPERATORS_CHARACTER_SET_HPP
#define BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_OPERATORS_CHARACTER_SET_HPP

#include <boost/mysql/character_set.hpp>

#include <iosfwd>

namespace boost {
namespace mysql {

bool operator==(character_set lhs, character_set rhs);
std::ostream& operator<<(std::ostream& os, character_set v);

}  // namespace mysql
}  // namespace boost

#endif
