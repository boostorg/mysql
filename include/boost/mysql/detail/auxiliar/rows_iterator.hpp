//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_AUXILIAR_ROWS_ITERATOR_HPP
#define BOOST_MYSQL_DETAIL_AUXILIAR_ROWS_ITERATOR_HPP

#include <cstdint>
#include <boost/mysql/row.hpp>
#include <boost/mysql/row_view.hpp>
#include <iterator>


namespace boost {
namespace mysql {
namespace detail {

template <class RowsType> // This can be either rows or rows_view
class rows_iterator
{
    const RowsType* obj_ {nullptr};
    std::size_t row_num_ {0};
public:
    using value_type = row;
    using reference = row_view;
    using pointer = row_view;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::random_access_iterator_tag;

    rows_iterator() = default;
    rows_iterator(const RowsType* obj, std::size_t rownum) noexcept : obj_(obj), row_num_(rownum) {}

    rows_iterator& operator++() noexcept { ++row_num_; return *this; }
    rows_iterator operator++(int) noexcept { auto res = *this; ++(*this); return res; }
    rows_iterator& operator--() noexcept { --row_num_; return *this; }
    rows_iterator operator--(int) noexcept { auto res = *this; --(*this); return res; }
    rows_iterator& operator+=(std::ptrdiff_t n) noexcept { row_num_ += n; return *this; }
    rows_iterator& operator-=(std::ptrdiff_t n) noexcept { row_num_ -= n; return *this; }
    rows_iterator operator+(std::ptrdiff_t n) const noexcept { return rows_iterator(obj_, row_num_ + n); }
    rows_iterator operator-(std::ptrdiff_t n) const noexcept { return rows_iterator(obj_, row_num_ - n); }
    std::ptrdiff_t operator-(rows_iterator rhs) const noexcept { return row_num_ - rhs.row_num_; }

    pointer operator->() const noexcept { return **this; }
    reference operator*() const noexcept { return (*obj_)[row_num_]; }
    reference operator[](std::ptrdiff_t i) const noexcept { return (*obj_)[row_num_ + i]; }

    bool operator==(rows_iterator rhs) const noexcept { return obj_ == rhs.obj_&& row_num_ == rhs.row_num_; }
    bool operator!=(rows_iterator rhs) const noexcept { return !(*this == rhs); }
    bool operator<(rows_iterator rhs) const noexcept { return row_num_ < rhs.row_num_; }
    bool operator<=(rows_iterator rhs) const noexcept { return row_num_ <= rhs.row_num_; }
    bool operator>(rows_iterator rhs) const noexcept { return row_num_ > rhs.row_num_; }
    bool operator>=(rows_iterator rhs) const noexcept { return row_num_ >= rhs.row_num_; }
};

template <class RowsType>
rows_iterator<RowsType> operator+(std::ptrdiff_t n, rows_iterator<RowsType> it) noexcept
{
    return it + n;
}

}
}
}



#endif
