//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_MAKE_TUPLE_ELEMENT_HPP
#define BOOST_MYSQL_MAKE_TUPLE_ELEMENT_HPP

#include <tuple>
#include <utility>

namespace boost {
namespace mysql {

/**
 * \brief Type trait that applies the transformation performed by `std::make_tuple` to a single element.
 * \details
 * For example: \n
 *   - `make_tuple_element_t<int>` yields `int` \n
 *   - `make_tuple_element_t<const int&>` yields `int` \n
 *   - `make_tuple_element_t<std::reference_wrapper<int>>` yields `int&` \n
 * \n
 * Consult the <a href="https://en.cppreference.com/w/cpp/utility/tuple/make_tuple">`std::make_tuple`</a> docs
 * for more info.
 */
template <class T>
using make_tuple_element_t = typename std::tuple_element<0, decltype(std::make_tuple(std::declval<T&&>()))>::
    type;

}  // namespace mysql
}  // namespace boost

#endif
