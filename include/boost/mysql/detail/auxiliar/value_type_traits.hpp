//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_AUXILIAR_VALUE_TYPE_TRAIS_HPP
#define BOOST_MYSQL_DETAIL_AUXILIAR_VALUE_TYPE_TRAIS_HPP

#include "boost/mysql/value.hpp"
#include <cstddef>
#include <iterator>
#include <type_traits>

namespace boost {
namespace mysql {
namespace detail {

struct value_type_helper
{
    struct placeholder {};

    template <typename T>
    static constexpr auto f(std::nullptr_t) -> typename std::iterator_traits<T>::value_type;
    
    template <typename T>
    static constexpr placeholder f(...);
};

struct iterator_category_helper
{
    struct placeholder {};

    template <typename T>
    static constexpr auto f(std::nullptr_t) -> typename std::iterator_traits<T>::iterator_category;
    
    template <typename T>
    static constexpr placeholder f(...);
};

template <class T>
struct is_value_forward_iterator : std::integral_constant<bool, 
    std::is_same<
        typename std::remove_cv<
            typename std::remove_reference<
                decltype(value_type_helper::f<T>(nullptr))
            >::type
        >::type,
        ::boost::mysql::value
    >::value &&
    std::is_base_of<
        std::forward_iterator_tag, 
        decltype(iterator_category_helper::f<T>(nullptr))
    >::value
>
{
};

struct begin_end_helper
{
    struct placeholder {};

    template <typename T>
    static constexpr auto begin(std::nullptr_t) -> decltype(std::begin(std::declval<const T&>()));

    template <typename T>
    static constexpr placeholder begin(...);

    template <typename T>
    static constexpr auto end(std::nullptr_t) -> decltype(std::end(std::declval<const T&>()));

    template <typename T>
    static constexpr placeholder end(...);
};

template <class T>
struct is_value_collection : std::integral_constant<bool,
    is_value_forward_iterator<decltype(begin_end_helper::begin<T>(nullptr))>::value &&
    is_value_forward_iterator<decltype(begin_end_helper::end<T>(nullptr))>::value
>
{
};

// Helpers
template <typename T>
using enable_if_value_forward_iterator = typename std::enable_if<is_value_forward_iterator<T>::value>::type;

template <typename T>
using enable_if_value_collection = typename std::enable_if<is_value_collection<T>::value>::type;


}
}
}


#endif