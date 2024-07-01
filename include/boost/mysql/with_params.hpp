//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_WITH_PARAMS_HPP
#define BOOST_MYSQL_WITH_PARAMS_HPP

#include <boost/mysql/constant_string_view.hpp>
#include <boost/mysql/format_sql.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/any_execution_request.hpp>
#include <boost/mysql/detail/format_sql.hpp>

#include <boost/core/ignore_unused.hpp>
#include <boost/core/span.hpp>

#include <array>
#include <tuple>
#include <type_traits>
#include <utility>

namespace boost {
namespace mysql {

// TODO: hide these
namespace detail {

template <class TupleType, std::size_t... I>
std::array<format_arg, std::tuple_size<typename std::remove_reference<TupleType>::type>::value>
ftuple_to_array_impl(TupleType&& t, mp11::index_sequence<I...>)
{
    boost::ignore_unused(t);  // MSVC gets confused for tuples of size 0
    return {{{string_view(), std::get<I>(std::forward<TupleType>(t))}...}};
}

template <class TupleType>
std::array<format_arg, std::tuple_size<typename std::remove_reference<TupleType>::type>::value> ftuple_to_array(
    TupleType&& t
)
{
    return ftuple_to_array_impl(
        std::forward<TupleType>(t),
        mp11::make_index_sequence<std::tuple_size<typename std::remove_reference<TupleType>::type>::value>()
    );
}

}  // namespace detail

template <class... Formattable>
struct with_params_t
{
    struct proxy
    {
        constant_string_view query;
        std::array<format_arg, sizeof...(Formattable)> args;

        operator detail::any_execution_request() const { return {query, args}; }
    };

    // TODO: make private
    struct impl_t
    {
        constant_string_view query;
        std::tuple<Formattable...> args;

        proxy make_request() const& { return {query, detail::ftuple_to_array(args)}; }
        proxy make_request() && { return {query, detail::ftuple_to_array(std::move(args))}; }

    } impl_;
};

template <BOOST_MYSQL_FORMATTABLE... Formattable>
with_params_t<Formattable...> with_params(constant_string_view query, Formattable&&... args)
{
    return {
        {query, std::make_tuple(std::forward<Formattable>(args)...)}
    };
}

}  // namespace mysql
}  // namespace boost

#endif
