//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_PFR_HPP
#define BOOST_MYSQL_PFR_HPP

#include <boost/mysql/string_view.hpp>
#include <boost/mysql/underlying_row.hpp>

#include <boost/mysql/detail/typing/row_traits.hpp>

#include <boost/pfr/core.hpp>
#include <boost/pfr/core_name.hpp>
#include <boost/pfr/traits.hpp>

#include <array>
#include <cstddef>
#include <string_view>
#include <utility>

namespace boost {
namespace mysql {

template <class T>
struct pfr_by_name;

// TODO: probably move this
namespace detail {

// PFR field names use std::string_view
template <std::size_t N>
constexpr std::array<string_view, N> to_name_table_storage(std::array<std::string_view, N> input) noexcept
{
    std::array<string_view, N> res;
    for (std::size_t i = 0; i < N; ++i)
        res[i] = input[i];
    return res;
}

template <class T>
constexpr auto pfr_names_storage = to_name_table_storage(pfr::names_as_array<T>());

template <class T>
class row_traits<pfr_by_name<T>, false>
{
    // TODO: C++20 guards
    // TODO: static assert that T is PFR-reflectable
public:
    using underlying_row_type = T;
    using field_types = decltype(pfr::structure_to_tuple(std::declval<const T&>()));

    static constexpr name_table_t name_table() noexcept { return pfr_names_storage<T>; }

    template <class F>
    static void for_each_member(T& to, F&& function)
    {
        pfr::for_each_field(to, std::forward<F>(function));
    }
};

}  // namespace detail

}  // namespace mysql
}  // namespace boost

#endif
