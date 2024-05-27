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
#include <boost/mysql/error_with_diagnostics.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/statement.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/pipeline.hpp>
#include <boost/mysql/detail/resultset_encoding.hpp>
#include <boost/mysql/detail/writable_field_traits.hpp>

#include <boost/assert.hpp>
#include <boost/core/span.hpp>
#include <boost/throw_exception.hpp>
#include <boost/variant2/variant.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace boost {
namespace mysql {

/**
 * \brief A variant-like type holding the response of a single pipeline stage.
 * \details
 * When running dynamic pipelines with \ref pipeline_request, this type is used
 * to hold individual stage responses.
 * \n
 * This is a variant-like type, similar to `boost::system::result`. At any point in time,
 * it can contain: \n
 *   \li A \ref statement. Will happen if the operation was a prepare statement that succeeded.
 *   \li A \ref results. Will happen if the operation was a query or statement execution that succeeded.
 *   \li A \ref errcode_with_diagnostics. Will happen if the operation failed, or if it succeeded but
 *       the operation doesn't yield a value (as in close statement, reset connection and set character set).
 */
class any_stage_response
{
    variant2::variant<errcode_with_diagnostics, statement, results> impl_;

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif

    bool has_error() const { return impl_.index() == 0u; }

    // TODO: move to compiled
    void check_has_results() const
    {
        if (!has_results())
        {
            BOOST_THROW_EXCEPTION(
                std::invalid_argument("any_stage_response::as_results: object doesn't contain results")
            );
        }
    }

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
    bool has_statement() const { return impl_.index() == 1u; }

    /**
     * \brief Returns true if the object contains a results.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    bool has_results() const { return impl_.index() == 2u; }

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
    errcode_with_diagnostics error() const&
    {
        return has_error() ? variant2::unsafe_get<0>(impl_) : errcode_with_diagnostics();
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
    errcode_with_diagnostics error() &&
    {
        return has_error() ? variant2::unsafe_get<0>(std::move(impl_)) : errcode_with_diagnostics();
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
    statement as_statement() const
    {
        // TODO: move to compiled?
        if (!has_statement())
        {
            BOOST_THROW_EXCEPTION(
                std::invalid_argument("any_stage_response::as_statement: object doesn't contain a statement")
            );
        }
        return variant2::unsafe_get<1>(impl_);
    }

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
    statement get_statement() const
    {
        BOOST_ASSERT(has_statement());
        return variant2::unsafe_get<1>(impl_);
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
        return variant2::unsafe_get<2>(impl_);
    }

    /// \copydoc as_results
    results&& as_results() &&
    {
        check_has_results();
        return variant2::unsafe_get<2>(std::move(impl_));
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
    const results& get_results() const&
    {
        BOOST_ASSERT(has_results());
        return variant2::unsafe_get<2>(impl_);
    }

    /// \copydoc get_results
    results&& get_results() &&
    {
        BOOST_ASSERT(has_results());
        return variant2::unsafe_get<2>(std::move(impl_));
    }
};

/**
 * \brief A dynamic pipeline request.
 * \details
 * Contains a collection of pipeline stages, fully describing the work to be performed
 * by a pipeline operation. The number of stages and their type is determined at runtime.
 * Call any of the `add_xxx` functions to append new stages to the request.
 * \n
 * If the number of stages and their type is known at compile time, prefer using
 * \ref static_pipeline_request, instead.
 * \n
 * Stage responses are read into a vector of \ref any_stage_response, which is variant-like.
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

    /**
     * \brief Adds a text query execution request to the pipeline.
     * \details
     * Creates a stage with effects equivalent to running `any_connection::execute(query)`.
     *
     * \par Exception safety
     * Basic guarantee. Memory allocations while composing the stage may throw.
     *
     * \par Object lifetimes
     * The string pointed by `query` is copied into the request buffer, so it does not need
     * to be kept alive after this function returns.
     */
    pipeline_request& add_execute(string_view query)
    {
        impl_.stages_.push_back(detail::serialize_query(impl_.buffer_, query));
        return *this;
    }

    /**
     * \brief Adds a prepared statement execution request to the pipeline.
     * \details
     * Creates a stage with effects equivalent to running `any_connection::execute(stmt.bind(params))`.
     * As opposed to \ref statement::bind, this function will throw an exception if the number of
     * provided parameters doesn't match `stmt.num_params()`.
     *
     * \par Exception safety
     * Basic guarantee. Memory allocations while composing the stage may throw.
     * \throws std::invalid_argument If `sizeof...(params) != stmt.num_params()`.
     *
     * \par Object lifetimes
     * Parameters are copied as required, and need not be kept alive after this function returns.
     */
    template <BOOST_MYSQL_WRITABLE_FIELD... WritableField>
    pipeline_request& add_execute(statement stmt, const WritableField&... params)
    {
        std::array<field_view, sizeof...(WritableField)> params_array{{detail::to_field(params)...}};
        add_execute_range(stmt, params_array);
        return *this;
    }

    /**
     * \brief Adds a prepared statement execution request to the pipeline.
     * \details
     * Creates a stage with effects equivalent to running
     * `any_connection::execute(stmt.bind(params.begin(), params.end()))`.
     * As opposed to \ref statement::bind, this function will throw an exception if the number of
     * provided parameters doesn't match `stmt.num_params()`.
     *
     * \par Exception safety
     * Basic guarantee. Memory allocations while composing the stage may throw.
     * \throws std::invalid_argument If `params.size() != stmt.num_params()`.
     *
     * \par Object lifetimes
     * Parameters are copied as required, and need not be kept alive after this function returns.
     */
    pipeline_request& add_execute_range(statement stmt, span<const field_view> params)
    {
        impl_.stages_.push_back(detail::serialize_execute_statement(impl_.buffer_, stmt, params));
        return *this;
    }

    /**
     * \brief Adds a statement preparation request to the pipeline.
     * \details
     * Creates a stage with effects equivalent to running
     * `any_connection::prepare_statement(statement_sql)`.
     *
     * \par Exception safety
     * Basic guarantee. Memory allocations while composing the stage may throw.
     *
     * \par Object lifetimes
     * The string pointed by `statement_sql` is copied into the request buffer, so it does not need
     * to be kept alive after this function returns.
     */
    pipeline_request& add_prepare_statement(string_view statement_sql)
    {
        impl_.stages_.push_back(detail::serialize_prepare_statement(impl_.buffer_, statement_sql));
        return *this;
    }

    /**
     * \brief Adds a close statement request to the pipeline.
     * \details
     * Creates a stage with effects equivalent to running
     * `any_connection::close_statement(stmt)`.
     *
     * \par Exception safety
     * Basic guarantee. Memory allocations while composing the stage may throw.
     */
    pipeline_request& add_close_statement(statement stmt)
    {
        impl_.stages_.push_back(detail::serialize_close_statement(impl_.buffer_, stmt.id()));
        return *this;
    }

    /**
     * \brief Adds a request to set the connection's character set to the pipeline.
     * \details
     * Creates a stage with effects equivalent to running
     * `any_connection::set_character_set(charset)`.
     *
     * \par Exception safety
     * Basic guarantee. Memory allocations while composing the stage may throw.
     * \throws std::invalid_argument If `charset.name` contains characters that are not allowed
     *         in a character set name. This is checked as a security hardening measure,
     *         and never happens with the character sets provided by this library.
     */
    pipeline_request& add_set_character_set(character_set charset)
    {
        impl_.stages_.push_back(detail::serialize_set_character_set(impl_.buffer_, charset));
        return *this;
    }

    /**
     * \brief Adds a reset connection request to the pipeline.
     * \details
     * Creates a stage with effects equivalent to running
     * `any_connection::reset_connection()`.
     *
     * \par Exception safety
     * Basic guarantee. Memory allocations while composing the stage may throw.
     */
    pipeline_request& add_reset_connection()
    {
        impl_.stages_.push_back(detail::serialize_reset_connection(impl_.buffer_));
        return *this;
    }

    /// The response type to use when running a pipeline with this request type,
    /// consisting of \ref any_stage_response objects.
    using response_type = std::vector<any_stage_response>;
};

}  // namespace mysql
}  // namespace boost

// TODO: can we move this to a separate header?
// Implementations
namespace boost {
namespace mysql {
namespace detail {

template <>
struct pipeline_response_traits<std::vector<any_stage_response>>
{
    using response_type = std::vector<any_stage_response>;

    static void setup(response_type& self, span<const pipeline_request_stage> request)
    {
        // Create as many response items as request stages
        self.resize(request.size());

        // Execution stage needs to be initialized to results objects.
        // Otherwise, clear any previous content
        for (std::size_t i = 0u; i < request.size(); ++i)
        {
            auto kind = request[i].kind;
            if (kind == pipeline_stage_kind::execute)
                access::get_impl(self[i]).emplace<results>();
            else
                access::get_impl(self[i]).emplace<errcode_with_diagnostics>();
        }
    }

    static execution_processor& get_processor(response_type& self, std::size_t idx)
    {
        BOOST_ASSERT(idx < self.size());
        auto& response_variant = access::get_impl(self[idx]);
        return access::get_impl(variant2::unsafe_get<2>(response_variant));
    }

    static void set_result(response_type& self, std::size_t idx, statement stmt)
    {
        BOOST_ASSERT(idx < self.size());
        access::get_impl(self[idx]) = stmt;
    }

    static void set_error(response_type& self, std::size_t idx, error_code ec, diagnostics&& diag)
    {
        BOOST_ASSERT(idx < self.size());
        access::get_impl(self[idx]).emplace<0>(ec, std::move(diag));
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
