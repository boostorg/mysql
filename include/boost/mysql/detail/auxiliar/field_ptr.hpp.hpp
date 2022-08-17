//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_AUXILIAR_FIELD_PTR_HPP_HPP
#define BOOST_MYSQL_DETAIL_AUXILIAR_FIELD_PTR_HPP_HPP

#include <vector>
#include <cstdint>
#include <boost/mysql/field.hpp>
#include <boost/mysql/field_view.hpp>

namespace boost {
namespace mysql {
namespace detail {

class field_ptr
{
    enum class type
    {
        field_view,
        field
    };

    type type_ {type::field_view};
    const void* ptr_ {};

    const void* incremented(std::ptrdiff_t n) const noexcept
    {
        if (type_ == type::field_view)
        {
            return static_cast<const field_view*>(ptr_) + n;
        }
        else
        {
            return static_cast<const field*>(ptr_) + n;
        }
    }
    field_view dereference() const noexcept
    {
        if (type_ == type::field_view)
        {
            return *static_cast<const field_view*>(ptr_);
        }
        else
        {
            return *static_cast<const field*>(ptr_);
        }
    }
    field_ptr(type t, const void* ptr) noexcept : type_(t), ptr_(ptr) {}
public:
    using value_type = field;
    using reference = field_view;
    using pointer = void const*;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::random_access_iterator_tag;

    field_ptr() noexcept = default;
    explicit field_ptr(const field_view* v) noexcept : type_(type::field_view), ptr_(v) {}
    explicit field_ptr(const field* v) noexcept : type_(type::field), ptr_(v) {}

    const field* as_field() const noexcept { assert(type_ == type::field); return static_cast<const field*>(ptr_); }
    const field_view* as_field_view() const noexcept { assert(type_ == type::field_view); return static_cast<const field_view*>(ptr_); }

    field_ptr& operator++() noexcept { ptr_ = incremented(1); return *this; }
    field_ptr operator++(int) noexcept { return field_ptr(type_, incremented(1)); }
    field_ptr& operator--() noexcept { ptr_ = incremented(-1); return *this; }
    field_ptr operator--(int) noexcept { return field_ptr(type_, incremented(-1)); }
    field_ptr& operator+=(std::ptrdiff_t n) noexcept { ptr_ = incremented(n); return *this; }
    field_ptr& operator-=(std::ptrdiff_t n) noexcept { ptr_ = incremented(-n); return *this; }
    field_ptr operator+(std::ptrdiff_t n) const noexcept { return field_ptr(type_, incremented(n)); }
    field_ptr operator-(std::ptrdiff_t n) const noexcept { return field_ptr(type_, incremented(-n)); }
    std::ptrdiff_t operator-(const field_ptr& rhs) const noexcept
    {
        if (type_ == type::field)
        {
            return static_cast<const field*>(ptr_) - static_cast<const field*>(rhs.ptr_);
        }
        else
        {
            return static_cast<const field_view*>(ptr_) - static_cast<const field_view*>(rhs.ptr_);
        }
    }
    field_view operator*() const noexcept { return dereference(); }
    field_view operator[](std::ptrdiff_t i) const noexcept { return (*this + i).dereference(); }
    bool operator==(const field_ptr& rhs) const noexcept { return type_ == rhs.type_ && ptr_ == rhs.ptr_; }
    bool operator!=(const field_ptr& rhs) const noexcept { return !(*this == rhs); }
    bool operator<(const field_ptr& rhs) const noexcept { return ptr_ < rhs.ptr_; }
    bool operator<=(const field_ptr& rhs) const noexcept { return ptr_ <= rhs.ptr_; }
    bool operator>(const field_ptr& rhs) const noexcept { return ptr_ > rhs.ptr_; }
    bool operator>=(const field_ptr& rhs) const noexcept { return ptr_ >= rhs.ptr_; }
};

inline field_ptr operator+(std::ptrdiff_t n, const field_ptr& ptr) noexcept { return ptr + n; }

}
}
}



#endif
