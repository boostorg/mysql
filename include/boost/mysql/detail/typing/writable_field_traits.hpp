//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_TYPING_WRITABLE_FIELD_TRAITS_HPP
#define BOOST_MYSQL_DETAIL_TYPING_WRITABLE_FIELD_TRAITS_HPP

#include <boost/mysql/field_view.hpp>
#include <boost/mysql/non_null.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/auxiliar/is_optional.hpp>
#include <boost/mysql/detail/auxiliar/void_t.hpp>
#include <boost/mysql/detail/config.hpp>

#include <string>
#include <type_traits>

namespace boost {
namespace mysql {
namespace detail {

// A type is a writable field if to_field(const T&) yields a field_view

// Types accepted by field_view's ctor.
template <class T>
typename std::enable_if<std::is_constructible<field_view, const T&>::value, field_view>::type to_field(
    const T& value
) noexcept
{
    return field_view(value);
}

template <class Traits, class Allocator>
field_view to_field(const std::basic_string<char, Traits, Allocator>& value) noexcept
{
    return field_view(string_view(value));
}

template <class Allocator>
field_view to_field(const std::vector<unsigned char, Allocator>& value) noexcept
{
    return field_view(blob_view(value));
}

template <class T>
field_view to_field(const non_null<T>& value) noexcept
{
    return to_field(value.value);
}

// Optional types
template <class T>
typename std::enable_if<
    is_optional<T>::value &&
        std::is_same<decltype(to_field(std::declval<typename T::value_type>())), field_view>::value,
    field_view>::type
to_field(const T& value) noexcept
{
    if (value.has_value())
        return to_field(value.value());
    else
        return field_view();
}

template <class T, class = void>
struct is_writable_field : std::false_type
{
};

template <class T>
struct is_writable_field<T, void_t<decltype(::boost::mysql::detail::to_field(std::declval<const T&>()))>>
    : std::true_type
{
};

// field_view_forward_iterator
template <typename T, typename = void>
struct is_field_view_forward_iterator : std::false_type
{
};

// clang-format off
template <typename T>
struct is_field_view_forward_iterator<T, void_t<
    typename std::enable_if<
        std::is_convertible<
            typename std::iterator_traits<T>::reference,
            field_view
        >::value
    >::type,
    typename std::enable_if<
        std::is_base_of<
            std::forward_iterator_tag, 
            typename std::iterator_traits<T>::iterator_category
        >::value
    >::type
>> : std::true_type { };
// clang-format on

#ifdef BOOST_MYSQL_HAS_CONCEPTS

template <class T>
concept field_view_forward_iterator = is_field_view_forward_iterator<T>::value;

#define BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR ::boost::mysql::detail::field_view_forward_iterator

#else  // BOOST_MYSQL_HAS_CONCEPTS

#define BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR class

#endif  // BOOST_MYSQL_HAS_CONCEPTS

// writable_field_tuple
template <class... T>
struct is_writable_field_tuple_impl : std::false_type
{
};

template <class... T>
struct is_writable_field_tuple_impl<std::tuple<T...>>
    : mp11::mp_all_of<mp11::mp_list<T...>, is_writable_field>
{
};

template <class Tuple>
struct is_writable_field_tuple : is_writable_field_tuple_impl<typename std::decay<Tuple>::type>
{
};

#ifdef BOOST_MYSQL_HAS_CONCEPTS

template <class T>
concept writable_field_tuple = is_writable_field_tuple<T>::value;

#define BOOST_MYSQL_WRITABLE_FIELD_TUPLE ::boost::mysql::detail::writable_field_tuple

#else  // BOOST_MYSQL_HAS_CONCEPTS

#define BOOST_MYSQL_WRITABLE_FIELD_TUPLE class

#endif  // BOOST_MYSQL_HAS_CONCEPTS

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
