//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_PFR_HPP
#define BOOST_MYSQL_IMPL_PFR_HPP

#pragma once

#include <boost/mysql/pfr.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/typing/row_traits.hpp>

#include <boost/pfr/core.hpp>
#include <boost/pfr/core_name.hpp>
#include <boost/pfr/traits.hpp>

#include <array>
#include <cstddef>
#include <string_view>
#include <type_traits>
#include <utility>

namespace boost {
namespace mysql {
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

// Workaround for https://github.com/boostorg/pfr/issues/165
constexpr std::array<string_view, 0u> to_name_table_storage(std::array<std::nullptr_t, 0u>) noexcept
{
    return {};
}

template <class T>
constexpr auto pfr_names_storage = to_name_table_storage(pfr::names_as_array<T>());

template <class T>
constexpr bool is_pfr_reflectable() noexcept
{
    // TODO: static assert that T is PFR-reflectable
    return std::is_class_v<T> && !std::is_const_v<T> && pfr::is_implicitly_reflectable_v<T, struct mysql_tag>;
}

template <class T>
class row_traits<pfr_by_name<T>, false>
{
    // TODO: C++20 guards
    static_assert(
        is_pfr_reflectable<T>(),
        "T needs to be a non-const object type that supports PFR reflection"
    );

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
