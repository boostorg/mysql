//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_ROWS_HPP
#define BOOST_MYSQL_ROWS_HPP

#include <boost/mysql/detail/auxiliar/rows_iterator.hpp>
#include <boost/mysql/detail/auxiliar/row_base.hpp>
#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/row_view.hpp>
#include <boost/mysql/row.hpp>

namespace boost {
namespace mysql {

class rows : private detail::row_base
{
public:
    using iterator = detail::rows_iterator<rows>;
    using const_iterator = iterator;
    using value_type = row;
    using reference = row_view;
    using const_reference = row_view;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    rows() = default;
    inline rows(const rows_view& view);
    rows(const rows&) = default;
    rows(rows&&) = default;
    rows& operator=(const rows&) = default;
    rows& operator=(rows&&) = default;
    ~rows() = default;

    // UB if rv comes from this: this_rows = rows_view(this_rows);
    inline rows& operator=(const rows_view& rv);

    iterator begin() const noexcept { return iterator(this, 0); }
    iterator end() const noexcept { return iterator(this, size()); }
    inline row_view at(std::size_t i) const;
    inline row_view operator[](std::size_t i) const noexcept;
    row_view front() const noexcept { return (*this)[0]; }
    row_view back() const noexcept { return (*this)[size() - 1]; }
    bool empty() const noexcept { return fields_.empty(); }
    std::size_t size() const noexcept { return num_columns_ == 0 ? 0 : fields_.size() / num_columns_; }
    operator rows_view() const noexcept { return rows_view(fields_.data(), fields_.size(), num_columns_); }
private:
    std::size_t num_columns_ {};
};


} // mysql
} // boost


#include <boost/mysql/impl/rows.ipp>


#endif
