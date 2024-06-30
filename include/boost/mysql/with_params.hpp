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

#include <boost/mysql/detail/format_sql.hpp>

#include <boost/core/span.hpp>

#include <array>
#include <cstddef>
#include <initializer_list>
#include <utility>

namespace boost {
namespace mysql {

template <std::size_t N>
struct with_params_t
{
    // TODO: make private
    constant_string_view query;
    std::array<format_arg, N> args;
};

struct with_params_range
{
    // TODO: make private
    constant_string_view query;
    span<const format_arg> args;
};

template <BOOST_MYSQL_FORMATTABLE... Args>
with_params_t<sizeof...(Args)> with_params(constant_string_view query, Args&&... args)
{
    return {query, {{{string_view(), std::forward<Args>(args)}...}}};
}

inline with_params_range with_params(constant_string_view query, std::initializer_list<format_arg> args)
{
    return {
        query,
        {args.begin(), args.size()}
    };
}

}  // namespace mysql
}  // namespace boost

#endif
