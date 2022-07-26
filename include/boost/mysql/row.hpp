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
#include <cstddef>
#include <initializer_list>
#include <iterator>

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
            values_(std::move(values)), buffer_(std::move(buffer)) {}; // TODO: hide this
    row(const row&) = delete; // TODO
    row(row&&) = default;
    row& operator=(const row&) = delete; // TODO
    row& operator=(row&&) = default;
    ~row() = default;

    using iterator = const value*;
    using const_iterator = iterator;
    // TODO: add other standard container members when we add field and field_view

    iterator begin() const noexcept { return values_.data(); }
    iterator end() const noexcept { return values_.data() + values_.size(); }
    value at(std::size_t i) const { return values_.at(i); }
    value operator[](std::size_t i) const noexcept { return values_[i]; }
    value front() const noexcept { return values_.front(); }
    value back() const noexcept { return values_.back(); }
    bool empty() const noexcept { return values_.empty(); }
    std::size_t size() const noexcept { return values_.size(); }

    iterator insert(iterator before, value v);
    iterator insert(iterator before, std::initializer_list<value> v);
    template <class FwdIt>
    iterator insert(iterator before, FwdIt first, FwdIt last);

    iterator replace(iterator pos, value v);
    iterator replace(iterator first, iterator last, std::initializer_list<value> v);
    template <class FwdIt>
    iterator replace(iterator first, iterator last, FwdIt other_first, FwdIt other_last);

    iterator erase(iterator pos);
    iterator erase(iterator first, iterator last);

    void push_back(value v);
    void pop_back();



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

    // TODO: hide these
    const std::vector<value>& values() const noexcept { return values_; }
    std::vector<value>& values() noexcept { return values_; }
    const detail::bytestring& buffer() const noexcept { return buffer_; }
    detail::bytestring& buffer() noexcept { return buffer_; }
};


class row_view
{
    const value* values_ {};
    std::size_t size_ {};
public:
    row_view() = default;
    row_view(const value* v, std::size_t size) noexcept : values_ {v}, size_{size} {}
    row_view(const row& r) noexcept :
        values_(r.begin()),
        size_(r.size())
    {
    }

    using iterator = const value*;
    using const_iterator = iterator;

    iterator begin() const noexcept { return values_; }
    iterator end() const noexcept { return values_ + size_; }
    value at(std::size_t i) const;
    value operator[](std::size_t i) const noexcept { return values_[i]; }
    value front() const noexcept { return values_[0]; }
    value back() const noexcept { return values_[size_ - 1]; }
    bool empty() const noexcept { return size_ == 0; }
    std::size_t size() const noexcept { return size_; }
};



class rows
{
    std::vector<value> values_;
    detail::bytestring buffer_;
    std::size_t num_columns_ {};
public:
    rows() = default;
    rows(std::size_t num_columns) noexcept : num_columns_(num_columns) {}
    rows(const rows&) = delete; // TODO
    rows(rows&&) = default;
    const rows& operator=(const rows&) = delete; // TODO
    rows& operator=(rows&&) = default;

    class iterator;
    using const_iterator = iterator;
    // TODO: add other standard container members

    iterator begin() const noexcept { return iterator(this, 0); }
    iterator end() const noexcept { return iterator(this, size()); }
    row_view at(std::size_t i) const { /* TODO: check idx */ return (*this)[i]; }
    row_view operator[](std::size_t i) const noexcept
    {
        std::size_t offset = num_columns_ * i;
        return row_view(values_.data() + offset, num_columns_);
    }
    row_view front() const noexcept { return (*this)[0]; }
    row_view back() const noexcept { return (*this)[size() - 1]; }
    bool empty() const noexcept { return values_.empty(); }
    std::size_t size() const noexcept { return values_.size() / num_columns_; }

    // TODO: hide these
    std::vector<value>& values() noexcept { return values_; }
    detail::bytestring& buffer() noexcept { return buffer_; }



    class iterator
    {
        const rows* obj_;
        std::size_t row_num_;
    public:
        using value_type = row_view; // TODO: should this be row?
        using reference = row_view;
        using pointer = void const*;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::bidirectional_iterator_tag;
        // TODO: this can be made random access

        iterator(const rows* obj, std::size_t rownum) noexcept : obj_(obj), row_num_(rownum) {}

        iterator& operator++() noexcept { ++row_num_; return *this; }
        iterator operator++(int) noexcept { return iterator(obj_, row_num_ + 1); }
        iterator& operator--() noexcept { --row_num_; return *this; }
        iterator operator--(int) noexcept { return iterator(obj_, row_num_ - 1); }
        reference operator*() const noexcept { return (*obj_)[row_num_]; }
        bool operator==(const iterator& rhs) const noexcept { return obj_ == rhs.obj_&& row_num_ == rhs.row_num_; }
        bool operator!=(const iterator& rhs) const noexcept { return !(*this == rhs); }
    };
};


class rows_view
{
    const value* values_ {};
    std::size_t num_values_ {};
    std::size_t num_columns_ {};
public:
    rows_view() = default;
    rows_view(const value* values, std::size_t num_values, std::size_t num_columns) noexcept :
        values_(values),
        num_values_(num_values),
        num_columns_(num_columns)
    {
    }

    class iterator;
    using const_iterator = iterator;
    // TODO: add other standard container members

    iterator begin() const noexcept { return iterator(this, 0); }
    iterator end() const noexcept { return iterator(this, size()); }
    row_view at(std::size_t i) const { /* TODO: check idx */ return (*this)[i]; }
    row_view operator[](std::size_t i) const noexcept
    {
        std::size_t offset = num_columns_ * i;
        return row_view(values_ + offset, num_columns_);
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
