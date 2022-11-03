//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_ROWS_HPP
#define BOOST_MYSQL_ROWS_HPP

#include <boost/mysql/detail/auxiliar/row_base.hpp>
#include <boost/mysql/detail/auxiliar/rows_iterator.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/row.hpp>
#include <boost/mysql/row_view.hpp>
#include <boost/mysql/rows_view.hpp>

namespace boost {
namespace mysql {

/**
 * \brief An owning, read-only sequence of rows.
 * \details
 * Models an owning, matrix-like container. Indexing a `rows` object (by using iterators,
 * \ref rows::at or \ref rows::operator[]) returns a \ref row_view object, representing a
 * single row. All rows in the collection are the same size (as given by \ref num_columns).
 *
 * A `rows` object owns a chunk of memory in which it stores its elements. The \ref rows_view
 * objects obtained on element access point into the `rows`' internal storage. These views (and any
 * \ref row_view and \ref field_view obtained from the former) behave
 * like references, and are valid as long as pointers, iterators and references into the `rows`
 * object remain valid.
 \n
 * Objects of this type can be populated by the \ref resultset::read_some and \ref
 * resultset::read_all function family. You may create a `rows` object directly to persist \ref
 * rows_view objects, too.
 \n
 * Although owning, `rows` is read-only. It's optimized for memory re-use in \ref
 * resultset::read_some and \ref resultset::read_all loops.
 */
class rows : private detail::row_base
{
public:
#ifdef BOOST_MYSQL_DOXYGEN
    /**
     * \brief A random access iterator to an element.
     * \details The exact type of the iterator is unspecified.
     */
    using iterator = __see_below__;
#else
    using iterator = detail::rows_iterator;
#endif

    /// \copydoc iterator
    using const_iterator = iterator;

    /**
     * \brief A type that can hold elements in this collection with value semantics.
     * \details Note that element accesors (like \ref rows_view::operator[]) return \ref reference
     * objects instead of `value_type` objects. You can use this type if you need an owning class.
     */
    using value_type = row;

    /// The reference type.
    using reference = row_view;

    /// \copydoc reference
    using const_reference = row_view;

    /// An unsigned integer type to represent sizes.
    using size_type = std::size_t;

    /// A signed integer type used to represent differences.
    using difference_type = std::ptrdiff_t;

    /**
     * \brief Construct an empty `rows` object.
     */
    rows() = default;

    /**
     * \brief Copy constructor.
     * \details `*this` lifetime will be independent of `other`'s.
     */
    rows(const rows& other) = default;

    /**
     * \brief Move constructor.
     * \details Iterators and references (including \ref rows_view's, \ref row_view's and \ref
     * field_view's) to elements in `other` remain valid.
     */
    rows(rows&& other) = default;

    /**
     * \brief Copy assignment.
     * \details `*this` lifetime will be independent of `other`'s. Iterators and references
     * (including \ref rows_view's, \ref row_view's and \ref field_view's) to elements in `*this`
     * are invalidated.
     */
    rows& operator=(const rows& other) = default;

    /**
     * \brief Move assignment.
     * \details Iterators and references (including \ref rows_view's \ref row_view's and \ref
     * field_view's) to elements in `*this` are invalidated. Iterators and references to elements in
     * `other` remain valid.
     */
    rows& operator=(rows&& other) = default;

    /**
     * \brief Destructor.
     * \details Iterators and references (including \ref rows_view's \ref row_view's and \ref
     * field_view's) to elements in `*this` are invalidated.
     */
    ~rows() = default;

    /**
     * \brief Constructs a rows object from a \ref rows_view.
     * \details `*this` lifetime will be independent of `r`'s (the contents of `r` will be copied
     * into `*this`).
     */
    inline rows(const rows_view& r);

    /**
     * \brief Replaces the contents with a \ref rows_view.
     * \details `*this` lifetime will be independent of `r`'s (the contents of `r` will be copied
     * into `*this`). Iterators and references (including \ref rows_view's \ref row_view's and \ref
     * field_view's) to elements in `*this` are invalidated.
     */
    inline rows& operator=(const rows_view& r);

    /// \copydoc rows_view::begin
    iterator begin() const noexcept { return iterator(fields_.data(), num_columns_, 0); }

    /// \copydoc rows_view::end
    iterator end() const noexcept { return iterator(fields_.data(), num_columns_, size()); }

    /// \copydoc rows_view::at
    inline row_view at(std::size_t i) const;

    /// \copydoc rows_view::operator[]
    inline row_view operator[](std::size_t i) const noexcept;

    /// \copydoc rows_view::front
    row_view front() const noexcept { return (*this)[0]; }

    /// \copydoc rows_view::back
    row_view back() const noexcept { return (*this)[size() - 1]; }

    /// \copydoc rows_view::empty
    bool empty() const noexcept { return fields_.empty(); }

    /// \copydoc rows_view::size
    std::size_t size() const noexcept
    {
        return num_columns_ == 0 ? 0 : fields_.size() / num_columns_;
    }

    /// \copydoc rows_view::num_columns
    std::size_t num_columns() const noexcept { return num_columns_; }

    /**
     * \brief Creates a \ref rows_view that references `*this`.
     * \details The returned view will be valid until any function that invalidates iterators and
     * references is invoked on `*this` or `*this` is destroyed.
     */
    operator rows_view() const noexcept
    {
        return rows_view(fields_.data(), fields_.size(), num_columns_);
    }

#ifndef BOOST_MYSQL_DOXYGEN
    // TODO: hide this
    using detail::row_base::clear;
#endif

private:
    std::size_t num_columns_{};
};

/**
 * \relates rows
 * \brief Equality operator.
 * \details The containers are considered equal if they have the same number of rows and
 * they all compare equal, as defined by \ref row_view::operator==.
 */
inline bool operator==(const rows& lhs, const rows& rhs) noexcept
{
    return rows_view(lhs) == rows_view(rhs);
}

/**
 * \relates rows
 * \brief Inequality operator.
 */
inline bool operator!=(const rows& lhs, const rows& rhs) noexcept { return !(lhs == rhs); }

/**
 * \relates rows
 * \copydoc rows::operator==(const rows&, const rows&)
 */
inline bool operator==(const rows_view& lhs, const rows& rhs) noexcept
{
    return lhs == rows_view(rhs);
}

/**
 * \relates rows
 * \copydoc rows::operator!=(const rows&, const rows&)
 */
inline bool operator!=(const rows_view& lhs, const rows& rhs) noexcept { return !(lhs == rhs); }

/**
 * \relates rows
 * \copydoc rows::operator==(const rows&, const rows&)
 */
inline bool operator==(const rows& lhs, const rows_view& rhs) noexcept
{
    return rows_view(lhs) == rhs;
}

/**
 * \relates rows
 * \copydoc rows::operator!=(const rows&, const rows&)
 */
inline bool operator!=(const rows& lhs, const rows_view& rhs) noexcept { return !(lhs == rhs); }

}  // namespace mysql
}  // namespace boost

#include <boost/mysql/impl/rows.ipp>

#endif
