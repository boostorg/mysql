//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_AUXILIAR_VALUE_TYPE_TRAITS_HPP
#define BOOST_MYSQL_DETAIL_AUXILIAR_VALUE_TYPE_TRAITS_HPP

#include <boost/mysql/value.hpp>
#include <boost/mysql/detail/auxiliar/void_t.hpp>
#include <cstddef>
#include <iterator>
#include <type_traits>

namespace boost {
namespace mysql {
namespace detail {

template <typename T, typename = void>
struct is_value_forward_iterator : std::false_type { };

template <typename T>
struct is_value_forward_iterator<T, void_t<
    typename std::enable_if<
        std::is_same<
            typename std::remove_reference<
                typename std::remove_cv<
                    typename std::iterator_traits<T>::value_type
                >::type
            >::type,
            ::boost::mysql::value
        >::value
    >::type,
    typename std::enable_if<
        std::is_base_of<
            std::forward_iterator_tag, 
            typename std::iterator_traits<T>::iterator_category
        >::value
    >::type
>> : std::true_type { };

template <typename T, typename = void>
struct is_value_collection : std::false_type {};

template <typename T>
struct is_value_collection<T, void_t<
    typename std::enable_if<
        is_value_forward_iterator<decltype(std::begin(std::declval<const T&>()))>::value
    >::type,
    typename std::enable_if<
        is_value_forward_iterator<decltype(std::end(std::declval<const T&>()))>::value
    >::type
>> : std::true_type {};


// Helpers
template <typename T>
using enable_if_value_forward_iterator = typename std::enable_if<is_value_forward_iterator<T>::value>::type;

template <typename T>
using enable_if_value_collection = typename std::enable_if<is_value_collection<T>::value>::type;


}
}
}


#endif