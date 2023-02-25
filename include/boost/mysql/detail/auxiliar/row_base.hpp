//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_AUXILIAR_ROW_BASE_HPP
#define BOOST_MYSQL_DETAIL_AUXILIAR_ROW_BASE_HPP

#include <boost/mysql/field_view.hpp>

#include <cstddef>
#include <vector>

namespace boost {
namespace mysql {
namespace detail {

// Base class providing implementation helpers for row and rows.
// Models a field_view vector with strings pointing into a
// single character buffer.
class row_base
{
public:
    row_base() = default;
    inline row_base(const field_view* fields, std::size_t size);
    inline row_base(const row_base&);
    row_base(row_base&&) = default;
    inline row_base& operator=(const row_base&);
    row_base& operator=(row_base&&) = default;
    ~row_base() = default;

    // Used by row/rows in assignment from view
    inline void assign(const field_view* fields, std::size_t size);

    // Internal use only
    inline void copy_strings();

    // Used by execute() to save strings into our buffer
    inline void copy_strings_as_offsets(std::size_t first, std::size_t num_fields);

    // Used by execute() to restore sv offsets into string views, once all rows have been read
    inline void offsets_to_string_views()
    {
        for (auto& f : fields_)
            field_view_access::offset_to_string_view(f, string_buffer_.data());
    }

    // Used by execute() as first thing to do
    inline void clear() noexcept
    {
        fields_.clear();
        string_buffer_.clear();
    }

    std::vector<field_view> fields_;

private:
    std::vector<unsigned char> string_buffer_;
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#include <boost/mysql/detail/auxiliar/impl/row_base.ipp>

#endif
