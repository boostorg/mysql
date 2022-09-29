//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_ROW_VIEW_HPP
#define BOOST_MYSQL_ROW_VIEW_HPP

#include <boost/mysql/field_view.hpp>
#include <boost/mysql/field.hpp>
#include <cstddef>
#include <iosfwd>
#include <iterator>

namespace boost {
namespace mysql {


class row_view
{
public:
    row_view() = default;

    using iterator = const field_view*;
    using const_iterator = iterator;
    using value_type = field;
    using reference = field_view;
    using const_reference = field_view;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    iterator begin() const noexcept { return fields_; }
    iterator end() const noexcept { return fields_ + size_; }
    inline field_view at(std::size_t i) const;
    field_view operator[](std::size_t i) const noexcept { return fields_[i]; }
    field_view front() const noexcept { return *fields_; }
    field_view back() const noexcept { return fields_[size_ - 1]; }
    bool empty() const noexcept { return size_ == 0; }
    std::size_t size() const noexcept { return size_; }

    // TODO: hide these
    row_view(const field_view* f, std::size_t size) noexcept : fields_ (f), size_(size) {}
private:
    const field_view* fields_ {};
    std::size_t size_ {};
};


/**
 * \relates row
 * \brief Compares two rows.
 */
inline bool operator==(const row_view& lhs, const row_view& rhs) noexcept;

/**
 * \relates row
 * \brief Compares two rows.
 */
inline bool operator!=(const row_view& lhs, const row_view& rhs) noexcept { return !(lhs == rhs); }

/**
 * \relates row
 * \brief Streams a row.
 */
inline std::ostream& operator<<(std::ostream& os, const row_view& value);

} // mysql
} // boost

#include <boost/mysql/impl/row_view.ipp>

#endif
