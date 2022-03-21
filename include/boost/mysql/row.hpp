//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_ROW_HPP
#define BOOST_MYSQL_ROW_HPP

#include <boost/mysql/detail/auxiliar/bytestring.hpp>
#include <boost/mysql/detail/auxiliar/container_equals.hpp>
#include <boost/mysql/value.hpp>
#include <boost/mysql/metadata.hpp>
#include <algorithm>

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
 * about these fields using [refmem resultset fields].
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
class row
{
    std::vector<value> values_;
    detail::bytestring buffer_;
public:
    row() = default;
    row(std::vector<value>&& values, detail::bytestring&& buffer) noexcept :
            values_(std::move(values)), buffer_(std::move(buffer)) {};
    row(const row&) = delete;
    row(row&&) = default;
    row& operator=(const row&) = delete;
    row& operator=(row&&) = default;
    ~row() = default;

    /// Accessor for the sequence of values.
    const std::vector<value>& values() const noexcept { return values_; }

    /// Accessor for the sequence of values.
    std::vector<value>& values() noexcept { return values_; }

    /**
     * \brief Clears the row object.
     * \details Clears the value array and the memory buffer associated to this row.
     * After calling this operation, [refmem row values] will be the empty array. Any
     * pointers, references and iterators to elements in [refmem row values] will be invalidated.
     * Any string values using the memory held by this row will also become invalid. 
     */
    void clear() noexcept
    {
        values_.clear();
        buffer_.clear();
    }

    // Private, do not use
    const detail::bytestring& buffer() const noexcept { return buffer_; }
    detail::bytestring& buffer() noexcept { return buffer_; }
};

/**
 * \relates row
 * \brief Compares two rows.
 */
inline bool operator==(const row& lhs, const row& rhs)
{
    return detail::container_equals(lhs.values(), rhs.values());
}

/**
 * \relates row
 * \brief Compares two rows.
 */
inline bool operator!=(const row& lhs, const row& rhs) { return !(lhs == rhs); }

/**
 * \relates row
 * \brief Streams a row.
 */
inline std::ostream& operator<<(std::ostream& os, const row& value)
{
    os << '{';
    const auto& arr = value.values();
    if (!arr.empty())
    {
        os << arr[0];
        for (auto it = std::next(arr.begin()); it != arr.end(); ++it)
        {
            os << ", " << *it;
        }
    }
    return os << '}';
}

} // mysql
} // boost



#endif
