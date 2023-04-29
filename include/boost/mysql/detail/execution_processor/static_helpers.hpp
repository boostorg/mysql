//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_EXECUTION_PROCESSOR_STATIC_HELPERS_HPP
#define BOOST_MYSQL_DETAIL_EXECUTION_PROCESSOR_STATIC_HELPERS_HPP

#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/typing/readable_field_traits.hpp>
#include <boost/mysql/detail/typing/row_traits.hpp>

#include <algorithm>
#include <cstddef>

namespace boost {
namespace mysql {
namespace detail {

// TODO: this requires C++14, assert it somehow

inline void reset_pos_map(std::size_t* pos_map, std::size_t size) noexcept
{
    for (std::size_t i = 0; i < size; ++i)
        pos_map[i] = pos_map_field_absent;
}

inline std::size_t find_field_by_name(
    const string_view* name_table,
    std::size_t size,
    string_view field_name
) noexcept
{
    const string_view* first = name_table;
    const string_view* last = first + size;
    auto it = std::find(first, last, field_name);
    if (it == last)
        return pos_map_field_absent;
    return it - first;
}

inline void fill_pos_map(
    const string_view* name_table,
    std::size_t* pos_map,
    std::size_t num_columns,
    std::size_t meta_index,
    string_view field_name
) noexcept
{
    if (name_table)
    {
        // We're mapping fields by name. Try to find where in our target struct
        // is the current field located
        std::size_t target_pos = find_field_by_name(name_table, num_columns, field_name);
        if (target_pos != pos_map_field_absent)
        {
            pos_map[target_pos] = meta_index;
        }
    }
    else
    {
        // We're mapping by position. Any extra trailing fields are discarded
        if (meta_index < num_columns)
        {
            pos_map[meta_index] = meta_index;
        }
    }
}

template <class... RowType>
constexpr array_wrapper<std::size_t, sizeof...(RowType)> num_columns_table{{get_row_size<RowType>()...}};

template <class... RowType>
constexpr std::size_t max_num_columns = (std::max)({get_row_size<RowType>()...});

template <class... RowType>
constexpr array_wrapper<meta_check_fn, sizeof...(RowType)> meta_check_vtable{{&meta_check<RowType>...}};

template <class... RowType>
constexpr array_wrapper<const string_view*, sizeof...(RowType)> name_table{
    {get_row_field_names<RowType>()...}};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
