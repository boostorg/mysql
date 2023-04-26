//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_AUXILIAR_IS_OPTIONAL_HPP
#define BOOST_MYSQL_DETAIL_AUXILIAR_IS_OPTIONAL_HPP

#include <boost/mysql/detail/auxiliar/void_t.hpp>

#include <type_traits>

namespace boost {
namespace mysql {
namespace detail {

template <class T, class = void>
struct is_optional : std::false_type
{
};

template <class T>
struct is_optional<
    T,
    void_t<
        typename std::enable_if<
            std::is_same<decltype(std::declval<const T&>().has_value()), bool>::value &&
            std::is_same<decltype(std::declval<const T&>().value()), const typename T::value_type&>::value &&
            std::is_same<decltype(std::declval<T&>().value()), typename T::value_type&>::value>::type,
        // we currently require all of our field types to be default-constructible, so this is good enough.
        // TODO: could be improved for writable types
        decltype(std::declval<T&>().emplace()),
        decltype(std::declval<T&>().reset())>> : std::true_type
{
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
