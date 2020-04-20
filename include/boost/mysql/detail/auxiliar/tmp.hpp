//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_AUXILIAR_TMP_HPP
#define BOOST_MYSQL_DETAIL_AUXILIAR_TMP_HPP

#include <type_traits>

namespace boost {
namespace mysql {
namespace detail {

template <typename T, typename Head, typename... Tail>
struct is_one_of
{
    static constexpr bool value = std::is_same_v<T, Head> || is_one_of<T, Tail...>::value;
};

template <typename T, typename Head>
struct is_one_of<T, Head>
{
    static constexpr bool value = std::is_same_v<T, Head>;
};


template <typename T, typename... Types>
constexpr bool is_one_of_v = is_one_of<T, Types...>::value;

} // detail
} // mysql
} // boost


#endif /* INCLUDE_BOOST_MYSQL_DETAIL_AUXILIAR_TMP_HPP_ */
