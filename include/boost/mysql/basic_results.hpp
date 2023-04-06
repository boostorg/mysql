//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_BASIC_RESULTS_HPP
#define BOOST_MYSQL_BASIC_RESULTS_HPP

#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/metadata_collection_view.hpp>
#include <boost/mysql/resultset.hpp>
#include <boost/mysql/resultset_view.hpp>
#include <boost/mysql/rows.hpp>
#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/auxiliar/access_fwd.hpp>
#include <boost/mysql/detail/auxiliar/results_iterator.hpp>
#include <boost/mysql/detail/protocol/execution_state_impl.hpp>

#include <boost/describe/class.hpp>

#include <cassert>
#include <cstddef>
#include <stdexcept>

namespace boost {
namespace mysql {

// TODO: this does not belong here
struct empty
{
};
BOOST_DESCRIBE_STRUCT(empty, (), ())

template <class... RowType>
class basic_results
{
public:
    template <std::size_t I>
    using row_type = boost::span<const typename std::tuple_element<I, std::tuple<RowType...>>::type>;

    /**
     * \brief Default constructor.
     * \details Constructs an empty results object, with `this->has_value() == false`.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    basic_results() = default;

    /**
     * \brief Copy constructor.
     * \par Exception safety
     * Strong guarantee. Internal allocations may throw.
     */
    basic_results(const basic_results& other) = default;

    /**
     * \brief Move constructor.
     * \par Exception safety
     * No-throw guarantee.
     *
     * \par Object lifetimes
     * View objects obtained from `other` using \ref rows and \ref meta remain valid.
     * Any other views and iterators referencing `other` are invalidated.
     */
    basic_results(basic_results&& other) = default;

    /**
     * \brief Copy assignment.
     * \par Exception safety
     * Basic guarantee. Internal allocations may throw.
     *
     * \par Object lifetimes
     * Views and iterators referencing `*this` are invalidated.
     */
    basic_results& operator=(const basic_results& other) = default;

    /**
     * \brief Move assignment.
     * \par Exception safety
     * Basic guarantee. Internal allocations may throw.
     *
     * \par Object lifetimes
     * View objects obtained from `other` using \ref rows and \ref meta remain valid.
     * Any other views and iterators referencing `other` are invalidated. Views and iterators
     * referencing `*this` are invalidated.
     */
    basic_results& operator=(basic_results&& other) = default;

    /// Destructor
    ~basic_results() = default;

    /**
     * \brief Returns whether the object holds a valid result.
     * \details Having `this->has_value()` is a precondition to call all data accessors.
     * Objects populated by \ref connection::execute and \ref connection::async_execute
     * are guaranteed to have `this->has_value() == true`.
     *
     * \par Exception safety
     * No-throw guarantee.
     *
     * \par Complexity
     * Constant.
     */
    bool has_value() const noexcept { return impl_.complete(); }

    /**
     * \brief Returns the rows retrieved by the SQL query.
     * \details
     * For operations returning more than one resultset, returns the rows
     * for the first resultset.
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
     *
     * \par Complexity
     * Constant.
     */
    template <std::size_t I = 0>
    row_type<I> rows() const noexcept
    {
        assert(has_value());
        return impl_.template get_rows<I>();
    }

    /**
     * \brief Returns metadata about the columns in the query.
     * \details
     * The returned collection will have as many \ref metadata objects as columns retrieved by
     * the SQL query, and in the same order.
     * \n
     * For operations returning more than one resultset, returns metadata
     * for the first resultset.
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
     *
     * \par Complexity
     * Constant.
     */
    template <std::size_t I = 0>
    metadata_collection_view meta() const noexcept
    {
        static_assert(I < sizeof...(RowType));
        assert(has_value());
        return impl_.get_meta(I);
    }

    /**
     * \brief Returns the number of rows affected by the executed SQL statement.
     * \details
     * For operations returning more than one resultset, returns the
     * first resultset's affected rows.
     *
     * \par Preconditions
     * `this->has_value() == true`
     *
     * \par Exception safety
     * No-throw guarantee.
     *
     * \par Complexity
     * Constant.
     */
    template <std::size_t I = 0>
    std::uint64_t affected_rows() const noexcept
    {
        static_assert(I < sizeof...(RowType));
        assert(has_value());
        return impl_.get_affected_rows(I);
    }

    /**
     * \brief Returns the last insert ID produced by the executed SQL statement.
     * \details
     * For operations returning more than one resultset, returns the
     * first resultset's last insert ID.
     *
     * \par Preconditions
     * `this->has_value() == true`
     *
     * \par Exception safety
     * No-throw guarantee.
     *
     * \par Complexity
     * Constant.
     */
    template <std::size_t I = 0>
    std::uint64_t last_insert_id() const noexcept
    {
        static_assert(I < sizeof...(RowType));
        assert(has_value());
        return impl_.get_last_insert_id(I);
    }

    /**
     * \brief Returns the number of warnings produced by the executed SQL statement.
     * \details
     * For operations returning more than one resultset, returns the
     * first resultset's warning count.
     *
     * \par Preconditions
     * `this->has_value() == true`
     *
     * \par Exception safety
     * No-throw guarantee.
     *
     * \par Complexity
     * Constant.
     */
    template <std::size_t I = 0>
    unsigned warning_count() const noexcept
    {
        static_assert(I < sizeof...(RowType));
        assert(has_value());
        return impl_.get_warning_count(I);
    }

    /**
     * \brief Returns additionat text information about the execution of the SQL statement.
     * \details
     * The format of this information is documented by MySQL <a
     * href="https://dev.mysql.com/doc/c-api/8.0/en/mysql-info.html">here</a>.
     * \n
     * The returned string always uses ASCII encoding, regardless of the connection's character set.
     * \n
     * For operations returning more than one resultset, returns the
     * first resultset's info.
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
     *
     * \par Complexity
     * Constant.
     */
    template <std::size_t I = 0>
    string_view info() const noexcept
    {
        static_assert(I < sizeof...(RowType));
        assert(has_value());
        return impl_.get_info(I);
    }

    /**
     * \brief Returns the output parameters of a stored procedure call.
     * \details
     * Relevant for `CALL` operations performed using prepared statements that
     * bind placeholders to `OUT` or `INOUT` parameters. Returns a row containing a field per
     * bound output parameter.
     * \n
     * If this operation had no output parameters (e.g. it wasn't a `CALL`), returns an empty row.
     *
     * \par Preconditions
     * `this->has_value() == true`
     *
     * \par Exception safety
     * No-throw guarantee.
     *
     * \par Object lifetimes
     * The returned reference and any other references obtained from it are valid as long as
     * `*this` is alive. Move operations invalidate references.
     *
     * \par Complexity
     * Linear on `this->size()`.
     */
    // TODO: this shouldn't be detail
    typename detail::get_out_params<RowType...>::type out_params() const noexcept
    {
        assert(has_value());
        return impl_.get_out_params();
    }

    // TODO: hide this
    detail::execution_state_iface& impl() noexcept { return impl_; }

private:
    detail::basic_results_impl<RowType...> impl_;
#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::basic_results_access;
#endif
};

struct detail::basic_results_access
{
    template <typename... RowType>
    static detail::basic_results_impl<RowType...>& get_impl(basic_results<RowType...>& obj) noexcept
    {
        return obj.impl_;
    }
};

}  // namespace mysql
}  // namespace boost

#endif
