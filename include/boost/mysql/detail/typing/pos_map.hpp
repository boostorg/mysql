//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_TYPING_POS_MAP_HPP
#define BOOST_MYSQL_DETAIL_TYPING_POS_MAP_HPP

#include <boost/mysql/field_view.hpp>
#include <boost/mysql/metadata.hpp>
#include <boost/mysql/metadata_collection_view.hpp>

#include <boost/assert.hpp>
#include <boost/config.hpp>

#include <cstddef>
#include <span>
#include <string_view>

namespace boost {
namespace mysql {
namespace detail {

// These functions map C++ type positions to positions to positions in the DB query

BOOST_INLINE_CONSTEXPR std::size_t pos_absent = static_cast<std::size_t>(-1);
using name_table_t = std::span<const std::string_view>;

inline bool has_field_names(name_table_t name_table) noexcept { return !name_table.empty(); }

inline void pos_map_reset(std::span<std::size_t> self) noexcept
{
    for (std::size_t i = 0; i < self.size(); ++i)
        self.data()[i] = pos_absent;
}

inline void pos_map_add_field(
    std::span<std::size_t> self,
    name_table_t name_table,
    std::size_t db_index,
    std::string_view field_name
) noexcept
{
    if (has_field_names(name_table))
    {
        BOOST_ASSERT(self.size() == name_table.size());

        // We're mapping fields by name. Try to find where in our target struct
        // is the current field located
        auto it = std::find(name_table.begin(), name_table.end(), field_name);
        if (it != name_table.end())
        {
            std::size_t cpp_index = it - name_table.begin();
            self[cpp_index] = db_index;
        }
    }
    else
    {
        // We're mapping by position. Any extra trailing fields are discarded
        if (db_index < self.size())
        {
            self[db_index] = db_index;
        }
    }
}

inline field_view map_field_view(
    std::span<const std::size_t> self,
    std::size_t cpp_index,
    std::span<const field_view> array
) noexcept
{
    BOOST_ASSERT(cpp_index < self.size());
    return array[self[cpp_index]];
}

inline const metadata& map_metadata(
    std::span<const std::size_t> self,
    std::size_t cpp_index,
    metadata_collection_view meta
) noexcept
{
    BOOST_ASSERT(cpp_index < self.size());
    return meta[self[cpp_index]];
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
