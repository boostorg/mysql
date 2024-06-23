//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_PIPELINE_HPP
#define BOOST_MYSQL_PIPELINE_HPP

#include <boost/mysql/character_set.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/statement.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/pipeline.hpp>
#include <boost/mysql/detail/writable_field_traits.hpp>

#include <boost/assert.hpp>
#include <boost/core/span.hpp>
#include <boost/variant2/variant.hpp>

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

namespace boost {
namespace mysql {

/** TODO: we can probably get rid of this
 * \brief A custom error type to be used with system::result.
 * \details
 * A custom error type containing an error code and a diagnostics object.
 * It can be used as the second template parameter of `boost::system::result`
 * (`boost::system::result<T, errcode_with_diagnostics>`).
 * \n
 * When `system::result::value()` throws, the exception type is \ref error_with_diagnostics,
 * containing the error code and diagnostics contained in this type.
 */
struct errcode_with_diagnostics
{
    /// The error code.
    error_code code;

    /// The diagnostics object.
    diagnostics diag;
};

/** TODO: rename to stage_response?
 * \brief (EXPERIMENTAL) A variant-like type holding the response of a single pipeline stage.
 * \details
 * When running dynamic pipelines with \ref pipeline_request, this type is used
 * to hold individual stage responses.
 * \n
 * This is a variant-like type, similar to `boost::system::result`. At any point in time,
 * it can contain: \n
 *   \li A \ref statement. Will happen if the stage was a prepare statement that succeeded.
 *   \li A \ref results. Will happen if the stage was a query or statement execution that succeeded.
 *   \li A \ref errcode_with_diagnostics. Will happen if the stage failed, or if it succeeded but
 *       it doesn't yield a value (as in close statement, reset connection and set character set).
 *
 * \par Experimental
 * This part of the API is experimental, and may change in successive
 * releases without previous notice.
 */
class any_stage_response
{
#ifndef BOOST_MYSQL_DOXYGEN
    struct
    {
        variant2::variant<errcode_with_diagnostics, statement, results> value;

        void emplace_results() { value.emplace<results>(); }
        void emplace_error() { value.emplace<errcode_with_diagnostics>(); }
        detail::execution_processor& get_processor()
        {
            return detail::access::get_impl(variant2::unsafe_get<2>(value));
        }
        void set_result(statement s) { value = s; }
        void set_error(error_code ec, diagnostics&& diag)
        {
            value.emplace<0>(errcode_with_diagnostics{ec, std::move(diag)});
        }
    } impl_;

    friend struct detail::access;
#endif

    bool has_error() const { return impl_.value.index() == 0u; }

    BOOST_MYSQL_DECL
    void check_has_results() const;

public:
    /**
     * \brief Default constructor.
     * \details
     * Constructs an object containing an empty error (a default-constructed \ref errcode_with_diagnostics).
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    any_stage_response() = default;

    /**
     * \brief Returns true if the object contains a statement.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    bool has_statement() const noexcept { return impl_.value.index() == 1u; }

    /**
     * \brief Returns true if the object contains a results.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    bool has_results() const noexcept { return impl_.value.index() == 2u; }

    /**
     * \brief Retrieves the contained error (const lvalue reference accessor).
     * \details
     * If `*this` contains an error, retrieves it by copying it.
     * Otherwise (if `this->has_statement() || this->has_results()`),
     * returns an empty (default-constructed) error.
     *
     * \par Exception safety
     * Strong guarantee. Memory allocations when copying the error's diagnostics may throw.
     */
    errcode_with_diagnostics error() const& noexcept
    {
        return has_error() ? variant2::unsafe_get<0>(impl_.value) : errcode_with_diagnostics();
    }

    /**
     * \brief Retrieves the contained error (rvalue reference accessor).
     * \details
     * If `*this` contains an error, retrieves it by moving it.
     * Otherwise (if `this->has_statement() || this->has_results()`),
     * returns an empty (default-constructed) error.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    errcode_with_diagnostics error() && noexcept
    {
        return has_error() ? variant2::unsafe_get<0>(std::move(impl_.value)) : errcode_with_diagnostics();
    }

    /**
     * \brief Retrieves the contained statement or throws an exception.
     * \details
     * If `*this` contains an statement (`this->has_statement() == true`),
     * retrieves it. Otherwise, throws an exception.
     *
     * \par Exception safety
     * Strong guarantee.
     * \throws std::invalid_argument If `*this` does not contain a statement.
     */
    BOOST_MYSQL_DECL
    statement as_statement() const;

    /**
     * \brief Retrieves the contained statement (unchecked accessor).
     * \details
     * If `*this` contains an statement,
     * retrieves it. Otherwise, the behavior is undefined.
     *
     * \par Preconditions
     * `this->has_statement() == true`
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    statement get_statement() const noexcept
    {
        BOOST_ASSERT(has_statement());
        return variant2::unsafe_get<1>(impl_.value);
    }

    /**
     * \brief Retrieves the contained results or throws an exception.
     * \details
     * If `*this` contains a `results` object (`this->has_results() == true`),
     * retrieves a reference to it. Otherwise, throws an exception.
     *
     * \par Exception safety
     * Strong guarantee. Throws on invalid input.
     * \throws std::invalid_argument If `this->has_results() == false`
     *
     * \par Object lifetimes
     * The returned reference is valid as long as `*this` is alive
     * and hasn't been assigned to.
     */
    const results& as_results() const&
    {
        check_has_results();
        return variant2::unsafe_get<2>(impl_.value);
    }

    /// \copydoc as_results
    results&& as_results() &&
    {
        check_has_results();
        return variant2::unsafe_get<2>(std::move(impl_.value));
    }

    /**
     * \brief Retrieves the contained results (unchecked accessor).
     * \details
     * If `*this` contains a `results` object, retrieves a reference to it.
     * Otherwise, the behavior is undefined.
     *
     * \par Preconditions
     * `this->has_results() == true`
     *
     * \par Exception safety
     * No-throw guarantee.
     *
     * \par Object lifetimes
     * The returned reference is valid as long as `*this` is alive
     * and hasn't been assigned to.
     */
    const results& get_results() const& noexcept
    {
        BOOST_ASSERT(has_results());
        return variant2::unsafe_get<2>(impl_.value);
    }

    /// \copydoc get_results
    results&& get_results() && noexcept
    {
        BOOST_ASSERT(has_results());
        return variant2::unsafe_get<2>(std::move(impl_.value));
    }
};

/**
 * \brief (EXPERIMENTAL) A dynamic pipeline request.
 * \details
 * Contains a collection of pipeline stages, fully describing the work to be performed
 * by a pipeline operation. The number of stages and their type is determined at runtime.
 * Call any of the `add_xxx` functions to append new stages to the request.
 * \n
 * If the number of stages and their type is known at compile time, prefer using
 * \ref static_pipeline_request, instead.
 * \n
 * Stage responses are read into a vector of \ref any_stage_response, which is variant-like.
 *
 * \par Experimental
 * This part of the API is experimental, and may change in successive
 * releases without previous notice.
 */
class pipeline_request
{
#ifndef BOOST_MYSQL_DOXYGEN
    struct impl_t
    {
        std::vector<std::uint8_t> buffer_;
        std::vector<detail::pipeline_request_stage> stages_;
    } impl_;

    friend struct detail::access;
#endif

public:
    /**
     * \brief Default constructor.
     * \details Constructs an empty pipeline request, with no stages.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    pipeline_request() = default;

    // TODO: document
    std::size_t size() const noexcept { return impl_.stages_.size(); }
    bool empty() const noexcept { return impl_.stages_.empty(); }

    /** TODO: review
     * \brief Constructs a stage to execute a text query.
     * \details
     * Adding `*this` to a pipeline creates a stage that will run
     * `query` as a SQL query, as per \ref any_connection::execute.
     *
     * \par Exception safety
     * No-throw guarantee.
     *
     * \par Object lifetimes
     * No copies of `query` are performed. This type should only be used
     * as a function argument to avoid lifetime issues.
     */
    BOOST_MYSQL_DECL
    pipeline_request& add_execute(string_view query);

    /** TODO: review
     * \brief Constructs a stage to execute a prepared statement.
     * \details
     * Adding `*this` to a pipeline creates a stage that will run
     * `stmt` bound to any parameters passed in `params`, as per \ref any_connection::execute
     * and \ref statement::bind. For example, `execute_stage(stmt, {42, "John"})` has
     * effects equivalent to `conn.execute(stmt.bind(42, "John"))`.
     * \n
     * If your statement doesn't take any parameters, pass an empty initializer list
     * as `params`.
     *
     * \par Exception safety
     * No-throw guarantee. \n
     * If the supplied number of parameters doesn't match the statement's number of parameters
     * (i.e. `stmt.num_params() != params.size()`), a `std::invalid_argument` exception will
     * be thrown when the stage is added to the pipeline (\ref pipeline_request::add or
     * \ref static_pipeline_request::static_pipeline_request).
     *
     * \par Object lifetimes
     * No copies of `params` or the values they point to are performed.
     * This type should only be used as a function argument to avoid lifetime issues.
     */
    template <BOOST_MYSQL_WRITABLE_FIELD... WritableField>
    pipeline_request& add_execute(statement stmt, const WritableField&... params)
    {
        std::array<field_view, sizeof...(WritableField)> params_arr{{detail::to_field(params)...}};
        return add_execute_range(stmt, params_arr);
    }

    /** TODO: review
     * \brief Constructs a stage to execute a prepared statement.
     * \details
     * Adding `*this` to a pipeline creates a stage that will run
     * `stmt` bound to the parameter range passed in `params`, as per \ref any_connection::execute
     * and \ref statement::bind. For example, `execute_stage(stmt, params_array)` has
     * effects equivalent to `conn.execute(stmt.bind(params_array.begin(), params_array.end()))`.
     * \n
     * Use this signature when the number of parameters that your statement takes is not
     * known at compile-time.
     *
     * \par Exception safety
     * No-throw guarantee. \n
     * If the supplied number of parameters doesn't match the statement's number of parameters
     * (i.e. `stmt.num_params() != params.size()`), a `std::invalid_argument` exception will
     * be raised when the stage is added to the pipeline (\ref pipeline_request::add or
     * \ref static_pipeline_request::static_pipeline_request).
     *
     * \par Object lifetimes
     * No copies of `params` or the values they point to are performed.
     * This type should only be used as a function argument to avoid lifetime issues.
     */
    BOOST_MYSQL_DECL
    pipeline_request& add_execute_range(statement stmt, span<const field_view> params);

    /** TODO: review
     * \brief Constructor.
     * \details
     * Adding a `prepare_statement_stage(stmt_sql)` to a pipeline
     * creates a stage with effects equivalent to `conn.prepare_statement(stmt_sql)`.
     *
     * \par Exception safety
     * No-throw guarantee.
     *
     * \par Object lifetimes
     * No copies of `stmt_sql` are performed. This type should only be used
     * as a function argument to avoid lifetime issues.
     */
    BOOST_MYSQL_DECL
    pipeline_request& add_prepare_statement(string_view stmt_sql);

    /** TODO: review
     * \brief Constructor.
     * \details
     * Adding a `close_statement_stage(stmt)` to a pipeline
     * creates a stage with effects equivalent to `conn.close_statement(stmt)`.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    BOOST_MYSQL_DECL pipeline_request& add_close_statement(statement stmt);

    /** TODO: review
     * \brief Constructor.
     * \details
     * Adding a `reset_connection_stage()` to a pipeline
     * creates a stage with effects equivalent to `conn.reset_connection()`.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    BOOST_MYSQL_DECL pipeline_request& add_reset_connection();

    /** TODO: review
     * \brief Constructor.
     * \details
     * Adding `*this` to a pipeline creates a stage with effects
     * equivalent to `conn.set_character_set(charset)`.
     *
     * \par Exception safety
     * No-throw guarantee. \n
     * If the supplied character set name is not valid (i.e. `charset.name` contains
     * non-ASCII characters), a `std::invalid_argument` exception will
     * be raised when the stage is added to the pipeline (\ref pipeline_request::add or
     * \ref static_pipeline_request::static_pipeline_request). This never happens with
     * the character sets provided by the library. The check is performed as a hardening measure.
     */
    BOOST_MYSQL_DECL pipeline_request& add_set_character_set(character_set charset);

    /**
     * \brief Removes all stages in the pipeline request, making the object empty again.
     * \details
     * Can be used to re-use a single request object for multiple pipeline operations.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    void clear() noexcept
    {
        impl_.buffer_.clear();
        impl_.stages_.clear();
    }
};

}  // namespace mysql
}  // namespace boost

#ifdef BOOST_MYSQL_HEADER_ONLY
#include <boost/mysql/impl/pipeline.ipp>
#endif

#endif
