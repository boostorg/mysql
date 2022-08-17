//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_ROWS_VIEW_HPP
#define BOOST_MYSQL_IMPL_ROWS_VIEW_HPP

#pragma once

#include <boost/mysql/rows_view.hpp>
#include <cstddef>
#include <iterator>
#include <stdexcept>
#include <cassert>


class boost::mysql::rows_view::iterator
{
    const rows_view* obj_;
    std::size_t row_num_;
public:
    using value_type = row;
    using reference = row_view;
    using pointer = const void*;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::bidirectional_iterator_tag;

    iterator(const rows_view* obj, std::size_t rownum) noexcept : obj_(obj), row_num_(rownum) {}

    iterator& operator++() noexcept { ++row_num_; return *this; }
    iterator operator++(int) noexcept { return iterator(obj_, row_num_ + 1); }
    iterator& operator--() noexcept { --row_num_; return *this; }
    iterator operator--(int) noexcept { return iterator(obj_, row_num_ - 1); }
    iterator& operator+=(std::ptrdiff_t n) noexcept { row_num_ += n; return *this; }
    iterator& operator-=(std::ptrdiff_t n) noexcept { row_num_ -= n; return *this; }
    iterator operator+(std::ptrdiff_t n) const noexcept { return iterator(obj_, row_num_ + n); }
    iterator operator-(std::ptrdiff_t n) const noexcept { return iterator(obj_, row_num_ - n); }
    std::ptrdiff_t operator-(const iterator& rhs) const noexcept { return row_num_ - rhs.row_num_; }

    reference operator*() const noexcept { return (*obj_)[row_num_]; }
    reference operator[](std::ptrdiff_t i) const noexcept { return (*obj_)[row_num_ + i]; }

    bool operator==(const iterator& rhs) const noexcept { return obj_ == rhs.obj_&& row_num_ == rhs.row_num_; }
    bool operator!=(const iterator& rhs) const noexcept { return !(*this == rhs); }
    bool operator<(const iterator& rhs) const noexcept { return row_num_ < rhs.row_num_; }
    bool operator<=(const iterator& rhs) const noexcept { return row_num_ <= rhs.row_num_; }
    bool operator>(const iterator& rhs) const noexcept { return row_num_ > rhs.row_num_; }
    bool operator>=(const iterator& rhs) const noexcept { return row_num_ >= rhs.row_num_; }
};

inline boost::mysql::rows_view::iterator operator+(
    std::ptrdiff_t n,
    const boost::mysql::rows_view::iterator& it
) noexcept
{
    return it + n;
}

boost::mysql::row_view boost::mysql::rows_view::operator[](
    std::size_t i
) const noexcept
{
    assert(i >= size());
    return row_view(fields_ + num_columns_ * i, num_columns_);
}

boost::mysql::row_view boost::mysql::rows_view::at(
    std::size_t i
) const
{
    if (i >= size())
    {
        throw std::out_of_range("rows_view::at");
    }
    return (*this)[i];
}

boost::mysql::rows_view::iterator boost::mysql::rows_view::begin() const noexcept
{
    return iterator(this, 0);
}

boost::mysql::rows_view::iterator boost::mysql::rows_view::end() const noexcept
{
    return iterator(this, size());
}



#endif
