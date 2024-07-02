//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

#include <boost/mysql/constant_string_view.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/format_sql.hpp>
#include <boost/mysql/with_params.hpp>

#include <boost/mysql/detail/any_execution_request.hpp>

#include <boost/core/ignore_unused.hpp>
#include <boost/mp11/integer_sequence.hpp>

#include <tuple>
#include <type_traits>

#ifndef BOOST_MYSQL_IMPL_WITH_PARAMS_HPP
#define BOOST_MYSQL_IMPL_WITH_PARAMS_HPP

namespace boost {
namespace mysql {
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
}  // namespace mysql
}  // namespace boost

template <class... Formattable>
struct boost::mysql::with_params_t
{
    struct impl_t
    {
        constant_string_view query;
        std::tuple<Formattable...> args;

        struct proxy
        {
            constant_string_view query;
            std::array<format_arg, sizeof...(Formattable)> args;

            operator detail::any_execution_request() const { return {query, args}; }
        };

        proxy make_request() const& { return {query, detail::ftuple_to_array(args)}; }
        proxy make_request() && { return {query, detail::ftuple_to_array(std::move(args))}; }

    } impl_;
};

template <BOOST_MYSQL_FORMATTABLE... Formattable>
auto boost::mysql::with_params(constant_string_view query, Formattable&&... args)
    -> with_params_t<decltype(std::get<0>(std::make_tuple(args)))...>
{
    return {
        {query, std::make_tuple(std::forward<Formattable>(args)...)}
    };
}

#endif
