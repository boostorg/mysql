//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_PFR_HPP
#define BOOST_MYSQL_PFR_HPP

#include <boost/mysql/get_row_type.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/typing/row_traits.hpp>

#include <boost/mp11/utility.hpp>
#include <boost/pfr/core.hpp>
#include <boost/pfr/core_name.hpp>
#include <boost/pfr/traits.hpp>
#include <boost/pfr/tuple_size.hpp>

#include <array>
#include <cstddef>
#include <string_view>
#include <tuple>
#include <utility>

namespace boost {
namespace mysql {

template <class T>
struct pfr_by_name;

template <class T>
struct get_row_type<pfr_by_name<T>>
{
    using type = T;
};

// TODO: probably move this
namespace detail {

struct mysql_pfr_tag
{
};

// TODO: we can make this better
template <class T>
constexpr bool is_static_row<pfr_by_name<T>> = true;

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

template <class... T>
std::tuple<mp11::mp_identity<T>...> tuple_to_identity(const std::tuple<T...>&);

template <class T>
class row_traits<pfr_by_name<T>, false>
{
public:
    // TODO: can we improve this?
    using types = decltype(tuple_to_identity(pfr::structure_to_tuple(std::declval<const T&>())));

    static constexpr std::size_t size() noexcept { return pfr::tuple_size_v<T>; }

    static constexpr name_table_t name_table() noexcept { return pfr_names_storage<T>; }

    static void parse(parse_functor& parser, T& to)
    {
        pfr::for_each_field(to, [&parser](auto& mem) { parser(mem); });
    }
};

}  // namespace detail

}  // namespace mysql
}  // namespace boost

#endif
