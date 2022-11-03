//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_ROW_HPP
#define BOOST_MYSQL_ROW_HPP

#include <boost/mysql/detail/auxiliar/row_base.hpp>
#include <boost/mysql/field.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/row_view.hpp>

#include <cstddef>
#include <iosfwd>
#include <vector>

namespace boost {
namespace mysql {

/**
 * \brief An owning, read-only sequence of fields.
 * \details
 * A `row` object owns a chunk of memory in which it stores its elements. On element access (using
 * iterators, \ref row::at or \ref row::operator[]) it returns \ref field_view's pointing into the
 * `row`'s internal storage. These views behave like references, and are valid as long as pointers,
 * iterators and references into the `row` remain valid.
 * \n
 * Objects of this type can be populated by the \ref resultset::read_one function family. You may
 * create a `row` directly to persist a \ref row_view, too.
 * \n
 * Although owning, `row` is read-only.
 * It's optimized for memory re-use in \ref resultset::read_one loops. If you need to mutate fields,
 * use a `std::vector<field>` instead (see \ref row_view::as_vector and \ref row::as_vector).
 */
class row : private detail::row_base
{
public:
#ifdef BOOST_MYSQL_DOXYGEN
    /**
     * \brief A random access iterator to an element.
     * \details The exact type of the iterator is unspecified.
     */
    using iterator = __see_below__;
#else
    using iterator = const field_view*;
#endif

    /// \copydoc iterator
    using const_iterator = const field_view*;

    /**
     * \brief A type that can hold elements in this collection with value semantics.
     * \details Note that element accesors (like \ref rows_view::operator[]) return \ref reference
     * objects instead of `value_type` objects. You can use this type if you need an owning class.
     */
    using value_type = field;

    /// The reference type.
    using reference = field_view;

    /// \copydoc reference
    using const_reference = field_view;

    /// An unsigned integer type to represent sizes.
    using size_type = std::size_t;

    /// A signed integer type used to represent differences.
    using difference_type = std::ptrdiff_t;

    /**
     * \brief Constructs an empty row.
     */
    row() = default;

    /**
     * \brief Copy constructor.
     * \details `*this` lifetime will be independent of `other`'s.
     */
    row(const row& other) = default;

    /**
     * \brief Move constructor.
     * \details Iterators and references (including \ref row_view's and \ref field_view's) to
     * elements in `other` remain valid.
     */
    row(row&& other) = default;

    /**
     * \brief Copy assignment.
     * \details `*this` lifetime will be independent of `other`'s. Iterators and references
     * (including \ref row_view's and \ref field_view's) to elements in `*this` are invalidated.
     */
    row& operator=(const row& other) = default;

    /**
     * \brief Move assignment.
     * \details Iterators and references (including \ref row_view's and \ref field_view's) to
     * elements in `*this` are invalidated. Iterators and references to elements in `other` remain
     * valid.
     */
    row& operator=(row&& other) = default;

    /**
     * \brief Destructor.
     * \details Iterators and references (including \ref row_view's and \ref field_view's) to
     * elements in `*this` are invalidated.
     */
    ~row() = default;

    /**
     * \brief Constructs a row from a \ref row_view.
     * \details `*this` lifetime will be independent of `r`'s (the contents of `r` will be copied
     * into `*this`).
     */
    row(row_view r) : detail::row_base(r.begin(), r.size()) {}

    /**
     * \brief Replaces the contents with a \ref row_view.
     * \details `*this` lifetime will be independent of `r`'s (the contents of `r` will be copied
     * into `*this`). Iterators and references (including \ref row_view's and \ref field_view's) to
     * elements in `*this` are invalidated.
     */
    row& operator=(row_view r)
    {
        // Protect against self-assignment. This is valid as long as we
        // don't implement sub-range operators (e.g. row_view[2:4])
        if (r.fields_ == fields_.data())
        {
            assert(r.size() == fields_.size());
        }
        else
        {
            assign(r.begin(), r.size());
        }
        return *this;
    }

    /// \copydoc row_view::begin
    const_iterator begin() const noexcept { return fields_.data(); }

    /// \copydoc row_view::end
    const_iterator end() const noexcept { return fields_.data() + fields_.size(); }

    /// \copydoc row_view::at
    field_view at(std::size_t i) const { return fields_.at(i); }

    /// \copydoc row_view::operator[]
    field_view operator[](std::size_t i) const noexcept { return fields_[i]; }

    /// \copydoc row_view::front
    field_view front() const noexcept { return fields_.front(); }

    /// \copydoc row_view::back
    field_view back() const noexcept { return fields_.back(); }

    /// \copydoc row_view::empty
    bool empty() const noexcept { return fields_.empty(); }

    /// \copydoc row_view::size
    std::size_t size() const noexcept { return fields_.size(); }

    /**
     * \brief Creates a \ref row_view that references `*this`.
     * \details The returned view will be valid until any function that invalidates iterators and
     * references is invoked on `*this` or `*this` is destroyed.
     */
    operator row_view() const noexcept { return row_view(fields_.data(), fields_.size()); }

    /// \copydoc row_view::as_vector
    template <class Allocator>
    void as_vector(std::vector<field, Allocator>& out) const
    {
        out.assign(begin(), end());
    }

    /// \copydoc row_view::as_vector
    std::vector<field> as_vector() const { return std::vector<field>(begin(), end()); }

#ifndef BOOST_MYSQL_DOXYGEN
    // TODO: hide this
    using detail::row_base::clear;
    using detail::row_base::copy_strings;
    std::vector<field_view>& fields() noexcept { return fields_; }
#endif
};

/**
 * \relates row
 * \brief Equality operator.
 * \details The containers are considered equal if they have the same number of elements and they
 * all compare equal, as defined by \ref field_view::operator==.
 */
inline bool operator==(const row& lhs, const row& rhs) noexcept
{
    return row_view(lhs) == row_view(rhs);
}

/**
 * \relates row
 * \brief Inequality operator.
 */
inline bool operator!=(const row& lhs, const row& rhs) { return !(lhs == rhs); }

/**
 * \relates row
 * \copydoc row::operator==(const row&,const row&)
 */
inline bool operator==(const row& lhs, const row_view& rhs) noexcept
{
    return row_view(lhs) == rhs;
}

/**
 * \relates row
 * \copydoc row::operator!=(const row&,const row&)
 */
inline bool operator!=(const row& lhs, const row_view& rhs) noexcept { return !(lhs == rhs); }

/**
 * \relates row
 * \copydoc row::operator==(const row&,const row&)
 */
inline bool operator==(const row_view& lhs, const row& rhs) noexcept
{
    return lhs == row_view(rhs);
}

/**
 * \relates row
 * \copydoc row::operator!=(const row&,const row&)
 */
inline bool operator!=(const row_view& lhs, const row& rhs) noexcept { return !(lhs == rhs); }

/**
 * \relates row
 * \brief Streams a row.
 */
inline std::ostream& operator<<(std::ostream& os, const row& r) { return os << row_view(r); }

}  // namespace mysql
}  // namespace boost

#endif
