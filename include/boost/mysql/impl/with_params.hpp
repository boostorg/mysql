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
#include <boost/mysql/detail/execution_concepts.hpp>

#include <boost/core/ignore_unused.hpp>
#include <boost/mp11/integer_sequence.hpp>

#include <tuple>
#include <utility>

#ifndef BOOST_MYSQL_IMPL_WITH_PARAMS_HPP
#define BOOST_MYSQL_IMPL_WITH_PARAMS_HPP

template <class... Formattable>
struct boost::mysql::with_params_t
{
    struct impl_t
    {
        constant_string_view query;
        std::tuple<Formattable...> args;
    } impl_;
};

template <BOOST_MYSQL_FORMATTABLE... Formattable>
auto boost::mysql::with_params(constant_string_view query, Formattable&&... args)
    -> mp11::mp_rename<decltype(std::make_tuple(std::forward<Formattable>(args)...)), with_params_t>
{
    return {
        {query, std::make_tuple(std::forward<Formattable>(args)...)}
    };
}

// Execution request traits
namespace boost {
namespace mysql {
namespace detail {

template <std::size_t N>
struct with_params_proxy
{
    constant_string_view query;
    std::array<format_arg, N> args;

    operator detail::any_execution_request() const { return any_execution_request({query, args}); }
};

template <class... T>
struct execution_request_traits<with_params_t<T...>>
{
    template <class WithParamsType, std::size_t... I>
    static with_params_proxy<sizeof...(T)> make_request_impl(WithParamsType&& input, mp11::index_sequence<I...>)
    {
        boost::ignore_unused(input);  // MSVC gets confused for tuples of size 0
        // clang-format off
        return {
            input.impl_.query,
            {{
                {
                    string_view(),
                    formattable_ref(std::get<I>(std::forward<WithParamsType>(input).impl_.args))
                }...
            }}
        };
        // clang-format on
    }

    // Allow the value category of the object to be deduced
    template <class WithParamsType>
    static with_params_proxy<sizeof...(T)> make_request(WithParamsType&& input, std::vector<field_view>&)
    {
        return make_request_impl(
            std::forward<WithParamsType>(input),
            mp11::make_index_sequence<sizeof...(T)>()
        );
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
