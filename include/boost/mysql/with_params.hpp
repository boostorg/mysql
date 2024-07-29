//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_WITH_PARAMS_HPP
#define BOOST_MYSQL_WITH_PARAMS_HPP

#include <boost/mysql/constant_string_view.hpp>

#include <boost/mysql/detail/format_sql.hpp>

#include <boost/mp11/detail/mp_rename.hpp>

#include <tuple>

namespace boost {
namespace mysql {

template <class... Formattable>
struct with_params_t;

template <BOOST_MYSQL_FORMATTABLE... Formattable>
auto with_params(constant_string_view query, Formattable&&... args)
    -> mp11::mp_rename<decltype(std::make_tuple(std::forward<Formattable>(args)...)), with_params_t>;

}  // namespace mysql
}  // namespace boost

#include <boost/mysql/impl/with_params.hpp>

#endif