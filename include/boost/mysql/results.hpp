//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_RESULTS_HPP
#define BOOST_MYSQL_RESULTS_HPP

#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/metadata_collection_view.hpp>
#include <boost/mysql/resultset_view.hpp>
#include <boost/mysql/rows.hpp>
#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/auxiliar/access_fwd.hpp>
#include <boost/mysql/detail/auxiliar/results_iterator.hpp>
#include <boost/mysql/detail/protocol/execution_state_impl.hpp>

#include <cassert>
#include <stdexcept>

namespace boost {
namespace mysql {

/**
 * \brief Holds the results of a SQL query.
 * \details The results (rows, metadata and additional info) are held in-memory.
 * \n
 * \par Thread safety
 * Distinct objects: safe. \n
 * Shared objects: unsafe. \n
 */
class results
{
public:
    using iterator = detail::results_iterator;

    /// \copydoc iterator
    using const_iterator = iterator;

    /// A type that can hold elements in this collection with value semantics.
    // TODO: what do we put here?
    // using value_type = field;

    /// The reference type.
    using reference = field_view;

    /// \copydoc reference
    using const_reference = field_view;

    /// An unsigned integer type to represent sizes.
    using size_type = std::size_t;

    /// A signed integer type used to represent differences.
    using difference_type = std::ptrdiff_t;

    /**
     * \brief Default constructor.
     * \details Constructs an empty results object, with `this->has_value() == false`.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    results() noexcept : impl_(true) {}

    /**
     * \brief Copy constructor.
     * \par Exception safety
     * Strong guarantee. Internal allocations may throw.
     */
    results(const results& other) = default;

    /**
     * \brief Move constructor.
     * \par Exception safety
     * No-throw guarantee.
     *
     * \par Object lifetimes
     * View objects obtained from `other` remain valid.
     */
    results(results&& other) = default;

    /**
     * \brief Copy assignment.
     * \par Exception safety
     * Basic guarantee. Internal allocations may throw.
     *
     * \par Object lifetimes
     * View objects referencing `*this` are invalidated.
     */
    results& operator=(const results& other) = default;

    /**
     * \brief Move assignment.
     * \par Exception safety
     * Basic guarantee. Internal allocations may throw.
     *
     * \par Object lifetimes
     * View objects referencing `other` remain valid. View objects
     * referencing `*this` are invalidated.
     */
    results& operator=(results&& other) = default;

    /// Destructor
    ~results() = default;

    /**
     * \brief Returns whether the object holds a valid result.
     * \details Having `this->has_value()` is a precondition to call all data accessors.
     * Objects populated by \ref connection::query, \ref connection::execute_statement or their async
     * counterparts are guaranteed to have `this->has_value() == true`.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    bool has_value() const noexcept { return impl_.complete(); }

    /**
     * \brief Returns the rows retrieved by the SQL query.
     * \par Preconditions
     * `this->has_value() == true`
     *
     * \par Exception safety
     * No-throw guarantee.
     *
     * \par Object lifetimes
     * This function returns a view object, with reference semantics. The returned view points into
     * memory owned by `*this`, and will be valid as long as `*this` or an object move-constructed
     * from `*this` are alive.
     */
    rows_view rows() const noexcept
    {
        assert(has_value());
        return impl_.get_rows(0);
    }

    /**
     * \brief Returns metadata about the columns in the query.
     * \details
     * The returned collection will have as many \ref metadata objects as columns retrieved by
     * the SQL query, and in the same order.
     *
     * \par Preconditions
     * `this->has_value() == true`
     *
     * \par Exception safety
     * No-throw guarantee.
     *
     * \par Object lifetimes
     * This function returns a view object, with reference semantics. The returned view points into
     * memory owned by `*this`, and will be valid as long as `*this` or an object move-constructed
     * from `*this` are alive.
     */
    metadata_collection_view meta() const noexcept
    {
        assert(has_value());
        return impl_.get_meta(0);
    }

    /**
     * \brief Returns the number of rows affected by the executed SQL statement.
     * \par Preconditions
     * `this->has_value() == true`
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    std::uint64_t affected_rows() const noexcept
    {
        assert(has_value());
        return impl_.get_affected_rows(0);
    }

    /**
     * \brief Returns the last insert ID produced by the executed SQL statement.
     * \par Preconditions
     * `this->has_value() == true`
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    std::uint64_t last_insert_id() const noexcept
    {
        assert(has_value());
        return impl_.get_last_insert_id(0);
    }

    /**
     * \brief Returns the number of warnings produced by the executed SQL statement.
     * \par Preconditions
     * `this->has_value() == true`
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    unsigned warning_count() const noexcept
    {
        assert(has_value());
        return impl_.get_warning_count(0);
    }

    /**
     * \brief Returns additionat text information about the execution of the SQL statement.
     * \details
     * The format of this information is documented by MySQL <a
     * href="https://dev.mysql.com/doc/c-api/8.0/en/mysql-info.html">here</a>.
     * \n
     * The returned string always uses ASCII encoding, regardless of the connection's character set.
     *
     * \par Preconditions
     * `this->has_value() == true`
     *
     * \par Exception safety
     * No-throw guarantee.
     *
     * \par Object lifetimes
     * This function returns a view object, with reference semantics. The returned view points into
     * memory owned by `*this`, and will be valid as long as `*this` or an object move-constructed
     * from `*this` are alive.
     */
    string_view info() const noexcept
    {
        assert(has_value());
        return impl_.get_info(0);
    }

    iterator begin() const noexcept { return iterator(&impl_, 0); }
    iterator end() const noexcept { return iterator(&impl_, size()); }
    inline resultset_view at(std::size_t i) const
    {
        if (i >= size())
            throw std::out_of_range("results::at: out of range");
        return resultset_view(impl_, i);
    }

    resultset_view operator[](std::size_t i) const noexcept
    {
        assert(i < size());
        return resultset_view(impl_, i);
    }

    resultset_view front() const noexcept { return (*this)[0]; }

    resultset_view back() const noexcept { return (*this)[size() - 1]; }

    bool empty() const noexcept { return size() == 0; }

    std::size_t size() const noexcept { return impl_.num_resultsets(); }

    row_view out_params() const noexcept { return impl_.get_out_params(); }

private:
    detail::execution_state_impl impl_;
#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::results_access;
#endif
};

}  // namespace mysql
}  // namespace boost

#include <boost/mysql/impl/results.hpp>

#endif
