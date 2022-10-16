//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_ROWS_VIEW_HPP
#define BOOST_MYSQL_ROWS_VIEW_HPP

#include <boost/mysql/detail/auxiliar/rows_iterator.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/row.hpp>
#include <boost/mysql/row_view.hpp>

#include <cstddef>

namespace boost {
namespace mysql {

class rows_view
{
public:
    rows_view() = default;

    using iterator = detail::rows_iterator<rows_view>;
    using const_iterator = iterator;
    using value_type = row;
    using reference = row_view;
    using const_reference = row_view;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    const_iterator begin() const noexcept { return iterator(this, 0); }
    const_iterator end() const noexcept { return iterator(this, size()); }
    inline row_view at(std::size_t i) const;
    inline row_view operator[](std::size_t i) const noexcept;
    row_view front() const noexcept { return (*this)[0]; }
    row_view back() const noexcept { return (*this)[size() - 1]; }
    bool empty() const noexcept { return num_values_ == 0; }
    std::size_t size() const noexcept
    {
        return (num_columns_ == 0) ? 0 : (num_values_ / num_columns_);
    }
    std::size_t num_columns() const noexcept { return num_columns_; }

    inline bool operator==(const rows_view& rhs) const noexcept;
    inline bool operator!=(const rows_view& rhs) const noexcept { return !(*this == rhs); }

    // TODO: hide this
    rows_view(const field_view* fields, std::size_t num_values, std::size_t num_columns) noexcept
        : fields_(fields), num_values_(num_values), num_columns_(num_columns)
    {
        assert(num_values % num_columns == 0);
    }

private:
    const field_view* fields_{};
    std::size_t num_values_{};
    std::size_t num_columns_{};

    friend class rows;
};

}  // namespace mysql
}  // namespace boost

#include <boost/mysql/impl/rows_view.ipp>

#endif
