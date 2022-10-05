//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_ROW_HPP
#define BOOST_MYSQL_ROW_HPP

#include <boost/mysql/field.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/row_view.hpp>
#include <boost/mysql/detail/auxiliar/row_base.hpp>
#include <cstddef>
#include <iosfwd>
#include <vector>


namespace boost {
namespace mysql {

/**
 * \brief Represents a row returned from a database operation.
 * \details A row is a collection of values, plus a buffer holding memory
 * for the string [reflink value]s.
 *
 * Call [refmem row values] to get the actual sequence of
 * [reflink value]s the row contains.
 *
 * There will be the same number of values and in the same order as fields
 * in the SQL query that produced the row. You can get more information
 * about these fields using [refmem resultset_base fields].
 *
 * If any of the values is a string, it will be represented as a `string_view`
 * pointing into the row's buffer. These string values will be valid as long as
 * the [reflink row] object containing the memory they point to is alive and valid. Concretely:
 * - Destroying the row object invalidates the string values.
 * - Move assigning against the row invalidates the string values.
 * - Calling [refmem row clear] invalidates the string values.
 * - Move-constructing a [reflink row] from the current row does **not**
 *   invalidate the string values.
 *
 * Default constructible and movable, but not copyable.
 */
class row : private detail::row_base
{
public:
    using iterator = const field_view*;
    using const_iterator = const field_view*;
    using value_type = field;
    using reference = field_view;
    using const_reference = field_view;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    row() = default;
    row(const row&) = default;
    row(row&&) = default;
    row& operator=(const row&) = default;
    row& operator=(row&&) = default;
    ~row() = default;

    row(row_view r) : detail::row_base(r.begin(), r.size()) {}

    // UB if r comes from this: this_row = row_view(this_row));
    row& operator=(row_view r) { assign(r.begin(), r.size()); return *this; }
    
    const_iterator begin() const noexcept { return fields_.data(); }
    const_iterator end() const noexcept { return fields_.data() + fields_.size(); }
    field_view at(std::size_t i) const { return fields_.at(i); }
    field_view operator[](std::size_t i) const noexcept { return fields_[i]; }
    field_view front() const noexcept { return fields_.front(); }
    field_view back() const noexcept { return fields_.back(); }
    bool empty() const noexcept { return fields_.empty(); }
    std::size_t size() const noexcept { return fields_.size(); }

    operator row_view() const noexcept
    {
        return row_view(fields_.data(), fields_.size());
    }

    template <class Allocator>
    void as_vector(std::vector<field, Allocator>& out) const { out.assign(begin(), end()); }

    std::vector<field> as_vector() const { return std::vector<field>(begin(), end()); }

    // TODO: hide this
    using detail::row_base::clear;
};


inline bool operator==(const row& lhs, const row& rhs) noexcept { return row_view(lhs) == row_view(rhs); }
inline bool operator!=(const row& lhs, const row& rhs) { return !(lhs == rhs); }

inline bool operator==(const row& lhs, const row_view& rhs) noexcept { return row_view(lhs) == rhs; }
inline bool operator!=(const row& lhs, const row_view& rhs) noexcept { return !(lhs == rhs); }

inline bool operator==(const row_view& lhs, const row& rhs) noexcept { return lhs == row_view(rhs); }
inline bool operator!=(const row_view& lhs, const row& rhs) noexcept { return !(lhs == rhs); }

inline std::ostream& operator<<(std::ostream& os, const row& r) { return os << row_view(r); }


} // mysql
} // boost


#endif
