//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TYPING_NON_NULL_HPP
#define BOOST_MYSQL_TYPING_NON_NULL_HPP

namespace boost {
namespace mysql {

template <class T>
struct non_null
{
    T value{};
};

}  // namespace mysql
}  // namespace boost

#endif
