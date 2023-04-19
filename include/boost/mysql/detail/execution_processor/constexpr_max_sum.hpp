//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_EXECUTION_PROCESSOR_CONSTEXPR_MAX_SUM_HPP
#define BOOST_MYSQL_DETAIL_EXECUTION_PROCESSOR_CONSTEXPR_MAX_SUM_HPP

#include <cstddef>

namespace boost {
namespace mysql {
namespace detail {

constexpr std::size_t get_max_impl(std::size_t lhs, std::size_t rhs) noexcept
{
    return lhs > rhs ? lhs : rhs;
}

template <std::size_t N>
constexpr std::size_t get_max(const std::size_t (&arr)[N], std::size_t i = 0)
{
    return i >= N ? 0 : get_max_impl(arr[i], get_max(arr, i + 1));
}

template <std::size_t N>
constexpr std::size_t get_sum(const std::size_t (&arr)[N], std::size_t i = 0)
{
    return i >= N ? 0 : arr[i] + get_sum(arr, i + 1);
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
