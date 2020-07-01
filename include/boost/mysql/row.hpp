//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_ROW_HPP
#define BOOST_MYSQL_ROW_HPP

#include "boost/mysql/detail/auxiliar/bytestring.hpp"
#include "boost/mysql/detail/auxiliar/container_equals.hpp"
#include "boost/mysql/value.hpp"
#include "boost/mysql/metadata.hpp"
#include <algorithm>

namespace boost {
namespace mysql {

/**
 * \brief Represents a row returned from a query.
 * \details Call [refmem row values] to get the actual sequence of
 * [reflinl value]s the row contains.
 *
 * There will be the same number of values and in the same order as fields
 * in the SQL query that produced the row. You can get more information
 * about these fields using [refmem resultset fields].
 *
 * If any of the values is a string, it will point to an externally owned piece of memory.
 * Thus, the [reflink row] class is not owning, as opposed to [reflink owning_row].
 * Note that, in any case, the sequence of [reflink value]s __is__ owned by the
 * row object. The distinction applies only to the memory pointed to by
 * string [reflink value]s.
 */
class row
{
    std::vector<value> values_;
public:
    /// Default and initializing constructor.
    row(std::vector<value>&& values = {}):
        values_(std::move(values)) {};

    /// Accessor for the sequence of values.
    const std::vector<value>& values() const noexcept { return values_; }

    /// Accessor for the sequence of values.
    std::vector<value>& values() noexcept { return values_; }
};

/**
 * \brief A row that owns a chunk of memory for its string values.
 * \details Default constructible and movable, but not copyable.
 */
class owning_row : public row
{
    detail::bytestring buffer_;
public:
    owning_row() = default;
    owning_row(std::vector<value>&& values, detail::bytestring&& buffer) :
            row(std::move(values)), buffer_(std::move(buffer)) {};
    owning_row(const owning_row&) = delete;
    owning_row(owning_row&&) = default;
    owning_row& operator=(const owning_row&) = delete;
    owning_row& operator=(owning_row&&) = default;
    ~owning_row() = default;
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
