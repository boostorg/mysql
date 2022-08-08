//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_ROWS_VIEW_HPP
#define BOOST_MYSQL_ROWS_VIEW_HPP

#include <boost/mysql/field_view.hpp>
#include <boost/mysql/row_view.hpp>
#include <stdexcept>

namespace boost {
namespace mysql {

class rows_view
{
    const field_view* fields_ {};
    std::size_t num_values_ {};
    std::size_t num_columns_ {};
public:
    rows_view() = default;
    rows_view(const field_view* fields, std::size_t num_values, std::size_t num_columns) noexcept :
        fields_(fields),
        num_values_(num_values),
        num_columns_(num_columns)
    {
    }

    class iterator;
    using const_iterator = iterator;
    // TODO: add other standard container members

    iterator begin() const noexcept { return iterator(this, 0); }
    iterator end() const noexcept { return iterator(this, size()); }
    row_view at(std::size_t i) const
    {
        if (i >= size())
        {
            throw std::out_of_range("rows_view::at");
        }
        return (*this)[i];
    }
    row_view operator[](std::size_t i) const noexcept
    {
        return row_view(fields_ + num_columns_ * i, num_columns_);
    }
    row_view front() const noexcept { return (*this)[0]; }
    row_view back() const noexcept { return (*this)[size() - 1]; }
    bool empty() const noexcept { return num_values_ == 0; }
    std::size_t size() const noexcept { return num_values_ / num_columns_; }

    class iterator
    {
        const rows_view* obj_;
        std::size_t row_num_;
    public:
        using value_type = row_view; // TODO: should this be row?
        using reference = row_view;
        using pointer = void const*;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::bidirectional_iterator_tag;
        // TODO: this can be made random access

        iterator(const rows_view* obj, std::size_t rownum) noexcept : obj_(obj), row_num_(rownum) {}

        iterator& operator++() noexcept { ++row_num_; return *this; }
        iterator operator++(int) noexcept { return iterator(obj_, row_num_ + 1); }
        iterator& operator--() noexcept { --row_num_; return *this; }
        iterator operator--(int) noexcept { return iterator(obj_, row_num_ - 1); }
        reference operator*() const noexcept { return (*obj_)[row_num_]; }
        bool operator==(const iterator& rhs) const noexcept { return obj_ == rhs.obj_&& row_num_ == rhs.row_num_; }
        bool operator!=(const iterator& rhs) const noexcept { return !(*this == rhs); }
    };
};


} // mysql
} // boost



#endif
