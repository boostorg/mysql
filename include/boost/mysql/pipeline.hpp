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
#include <boost/mysql/detail/pipeline_concepts.hpp>
#include <boost/mysql/detail/writable_field_traits.hpp>

#include <boost/assert.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/core/span.hpp>
#include <boost/mp11/tuple.hpp>
#include <boost/mp11/utility.hpp>
#include <boost/system/result.hpp>
#include <boost/variant2/variant.hpp>

#include <array>
#include <cstdint>
#include <initializer_list>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace boost {
namespace mysql {

#ifndef BOOST_MYSQL_DOXYGEN
namespace detail {
struct pipeline_stage_access
{
    template <class PipelineStageType>
    static pipeline_request_stage create(PipelineStageType stage, std::vector<std::uint8_t>& buff)
    {
        return stage.create(buff);
    }
};
}  // namespace detail
#endif

/**
 * \brief (EXPERIMENTAL) Pipeline stage type to execute text queries and prepared statements.
 * \details
 * To be used with \ref pipeline_request or \ref static_pipeline_request.
 * Has effects equivalent to \ref any_connection::execute.
 *
 * \par Object lifetimes
 * Like all stage types, this is a view type. Parameters required to run the stage
 * are stored internally as views. Objects of this type should only be used as
 * function or constructor parameters.
 *
 * \par Experimental
 * This part of the API is experimental, and may change in successive
 * releases without previous notice.
 */
class execute_stage
{
public:
    /**
     * \brief A single statement parameter, to be used in initializer lists.
     * \details
     * Applies lightweight type-erasure to types satisfying the `WritableField` concept,
     * so they can be passed in initializer lists.
     *
     * \par Object lifetimes
     * This is a view type, and should only be used in initializer lists, as a function argument.
     */
    class statement_param_arg
    {
        field_view impl_;
#ifndef BOOST_MYSQL_DOXYGEN
        friend struct detail::access;
#endif
    public:
        /**
         * \brief Constructor.
         * \details
         * Can be passed any type satisfying `WritableField`.
         *
         * \par Exception safety
         * No-throw guarantee.
         *
         * \par Object lifetimes
         * No copies of `f` are performed. This type should only be used in initializer lists, to avoid
         * lifetime issues.
         */
        template <
            BOOST_MYSQL_WRITABLE_FIELD WritableField
#ifndef BOOST_MYSQL_DOXYGEN
            ,
            class = typename std::enable_if<detail::is_writable_field<WritableField>::value>::type
#endif
            >
        statement_param_arg(const WritableField& f) noexcept : impl_(detail::to_field(f))
        {
        }
    };

    /**
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
    execute_stage(string_view query) noexcept : type_(type_query), data_(query) {}

    /**
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
    execute_stage(statement stmt, std::initializer_list<statement_param_arg> params) noexcept
        : type_(type_stmt_tuple), data_(stmt, {params.begin(), params.size()})
    {
    }

    /**
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
    execute_stage(statement stmt, span<const field_view> params) noexcept
        : type_(type_stmt_range), data_(stmt, params)
    {
    }

    /**
     * \brief The response type used by the static interface.
     * \details
     * A `boost::system::result` that contains a \ref results on success and
     * a \ref errcode_with_diagnostics on failure.
     */
    using response_type = system::result<results, errcode_with_diagnostics>;

private:
    enum type_t
    {
        type_query,
        type_stmt_tuple,
        type_stmt_range
    };

    union data_t
    {
        string_view query;
        struct
        {
            statement stmt;
            span<const statement_param_arg> params;
        } stmt_tuple;
        struct
        {
            statement stmt;
            span<const field_view> params;
        } stmt_range;

        data_t(string_view q) noexcept : query(q) {}
        data_t(statement s, span<const statement_param_arg> params) noexcept : stmt_tuple{s, params} {}
        data_t(statement s, span<const field_view> params) noexcept : stmt_range{s, params} {}
    };

    type_t type_;
    data_t data_;

    BOOST_MYSQL_DECL
    detail::pipeline_request_stage create(std::vector<std::uint8_t>& buffer) const;

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::pipeline_stage_access;
#endif
};

/**
 * \brief (EXPERIMENTAL) Pipeline stage type to prepare statements.
 * \details
 * To be used with \ref pipeline_request or \ref static_pipeline_request.
 * Has effects equivalent to \ref any_connection::prepare_statement.
 *
 * \par Object lifetimes
 * Like all stage types, this is a view type. Parameters required to run the stage
 * are stored internally as views. Objects of this type should only be used as
 * function or constructor parameters.
 *
 * \par Experimental
 * This part of the API is experimental, and may change in successive
 * releases without previous notice.
 */
class prepare_statement_stage
{
    string_view stmt_sql_;

    BOOST_MYSQL_DECL
    detail::pipeline_request_stage create(std::vector<std::uint8_t>& buffer) const;

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::pipeline_stage_access;
#endif

public:
    /**
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
    prepare_statement_stage(string_view stmt_sql) noexcept : stmt_sql_(stmt_sql) {}

    /**
     * \brief The response type used by the static interface.
     * \details
     * A `boost::system::result` that contains a \ref statement on success and
     * a \ref errcode_with_diagnostics on failure.
     */
    using response_type = system::result<statement, errcode_with_diagnostics>;
};

/**
 * \brief (EXPERIMENTAL) Pipeline stage type to close statements.
 * \details
 * To be used with \ref pipeline_request or \ref static_pipeline_request.
 * Has effects equivalent to \ref any_connection::close_statement.
 *
 * \par Experimental
 * This part of the API is experimental, and may change in successive
 * releases without previous notice.
 */
class close_statement_stage
{
    std::uint32_t stmt_id_;

    BOOST_MYSQL_DECL
    detail::pipeline_request_stage create(std::vector<std::uint8_t>& buffer) const;

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::pipeline_stage_access;
#endif

public:
    /**
     * \brief Constructor.
     * \details
     * Adding a `close_statement_stage(stmt)` to a pipeline
     * creates a stage with effects equivalent to `conn.close_statement(stmt)`.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    close_statement_stage(statement stmt) noexcept : stmt_id_(stmt.id()) {}

    /// The response type used by the static interface.
    using response_type = errcode_with_diagnostics;
};

/**
 * \brief (EXPERIMENTAL) Pipeline stage type to reset session state.
 * \details
 * To be used with \ref pipeline_request or \ref static_pipeline_request.
 * Has effects equivalent to \ref any_connection::reset_connection.
 *
 * \par Experimental
 * This part of the API is experimental, and may change in successive
 * releases without previous notice.
 */
class reset_connection_stage
{
    BOOST_MYSQL_DECL
    detail::pipeline_request_stage create(std::vector<std::uint8_t>& buffer) const;

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::pipeline_stage_access;
#endif

public:
    /**
     * \brief Constructor.
     * \details
     * Adding a `reset_connection_stage()` to a pipeline
     * creates a stage with effects equivalent to `conn.reset_connection()`.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    reset_connection_stage() = default;

    /// The response type used by the static interface.
    using response_type = errcode_with_diagnostics;
};

/**
 * \brief (EXPERIMENTAL) Pipeline stage type to set the connection's character set.
 * \details
 * To be used with \ref pipeline_request or \ref static_pipeline_request.
 * Has effects equivalent to \ref any_connection::set_character_set.
 *
 * \par Experimental
 * This part of the API is experimental, and may change in successive
 * releases without previous notice.
 */
class set_character_set_stage
{
    character_set charset_;

    BOOST_MYSQL_DECL
    detail::pipeline_request_stage create(std::vector<std::uint8_t>& buffer) const;

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::pipeline_stage_access;
#endif

public:
    /**
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
    set_character_set_stage(character_set charset) noexcept : charset_(charset) {}

    /// The response type used by the static interface.
    using response_type = errcode_with_diagnostics;
};

/**
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

        detail::pipeline_request_view to_view() const { return {buffer_, stages_}; }
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
     * \brief Adds a stage to the pipeline request.
     * \details
     * Adds a stage with effects depending on the passed type. `PipelineStageType`
     * should be one of: \n
     *    \li \ref execute_stage
     *    \li \ref prepare_statement_stage
     *    \li \ref close_statement_stage
     *    \li \ref reset_connection_stage
     *    \li \ref set_character_set_stage
     *
     * \par Exception safety
     * Basic guarantee. Memory allocations while composing the request may throw.
     * Throws if `stage` contains invalid parameters. Consult individual stage descriptions for details.
     * \throws std::invalid_argument If `stage` contains invalid parameters.
     *
     * \par Object lifetimes
     * All arguments are copied into the request buffer. Once this function returns,
     * `stage` and any values that `stage` points to need not be kept alive.
     */
    template <BOOST_MYSQL_PIPELINE_STAGE_TYPE PipelineStageType>
    pipeline_request& add(PipelineStageType stage)
    {
        static_assert(detail::is_pipeline_stage_type<PipelineStageType>::value, "Invalid PipelineStageType");
        impl_.stages_.push_back(detail::pipeline_stage_access::create(stage, impl_.buffer_));
        return *this;
    }

    /**
     * \brief The response type to use when running a pipeline with this request type, consisting of \ref
     * any_stage_response objects.
     */
    using response_type = std::vector<any_stage_response>;
};

/**
 * \brief (EXPERIMENTAL) A static pipeline request type.
 * \details
 * Contains a collection of pipeline stages, fully describing the work to be performed
 * by a pipeline operation. The pipeline contains as many stages as `PipelineStageType`
 * template parameters are passed to this type.
 * \n
 * The following stage types may be used: \n
 *    \li \ref execute_stage
 *    \li \ref prepare_statement_stage
 *    \li \ref close_statement_stage
 *    \li \ref reset_connection_stage
 *    \li \ref set_character_set_stage
 * \n
 * Stage responses are read into `std::tuple` objects, with individual elements depending
 * on stage types. See \ref response_type for more info.
 * \n
 * To create instances of this class without specifying template parameters,
 * you can use \ref make_pipeline_request. If you're on C++17 or above, appropriate class deduction
 * guides are also provided.
 *
 * \par Experimental
 * This part of the API is experimental, and may change in successive
 * releases without previous notice.
 */
template <BOOST_MYSQL_PIPELINE_STAGE_TYPE... PipelineStageType>
class static_pipeline_request
{
    static_assert(sizeof...(PipelineStageType) > 0u, "A pipeline should have one stage, at least");
    static_assert(
        mp11::mp_all_of<mp11::mp_list<PipelineStageType...>, detail::is_pipeline_stage_type>::value,
        "Some pipeline stage types are not valid"
    );

    struct impl_t
    {
        std::vector<std::uint8_t> buffer_;
        std::array<detail::pipeline_request_stage, sizeof...(PipelineStageType)> stages_;

        detail::pipeline_request_view to_view() const { return {buffer_, stages_}; }

        impl_t(const PipelineStageType&... args)
            : stages_{{detail::pipeline_stage_access::create(args, buffer_)...}}
        {
        }
    } impl_;

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif

public:
    /**
     * \brief Constructor.
     * \details
     * Takes as many arguments as stages the pipeline has. Stage types include
     * all the parameters required to fully compose the request.
     *
     * \par Exception safety
     * Strong guarantee. Memory allocations while composing the request may throw.
     * Supplying steps with invalid arguments also throws.
     * See individual stage descriptions for possible error conditions.
     * \throws std::invalid_argument If any of `stages` contain invalid parameters.
     *
     * \par Object lifetimes
     * Parameters are stored as a serialized request on the constructed object.
     * The `stages` objects, and any other objects they point to, need not be kept alive
     * once the constructor returns.
     */
    static_pipeline_request(const PipelineStageType&... stages) : impl_(stages...) {}

    /**
     * \brief The response type to use when running a pipeline with this request type.
     * \details
     * Has the same number of elements as pipeline stages. Consult each stage
     * `response_type` typedef for more info.
     */
    using response_type = std::tuple<typename PipelineStageType::response_type...>;
};

/**
 * \brief (EXPERIMENTAL) Creates a static_pipeline_request object.
 * \details
 * This factory function can be used to avoid having to explicitly specify
 * template parameters on the request type. For C++17 code and later,
 * you can directly use \ref static_pipeline_request constructor, instead.
 *
 * \par Exception safety
 * Strong guarantee. Memory allocations while composing the request may throw.
 * Supplying steps with invalid arguments also throws.
 * \throws std::invalid_argument If `stages` contains a stage with invalid parameters.
 *         See individual stage descriptions for possible error conditions.
 *
 * \par Object lifetimes
 * The `stages` objects, and any other objects they point to, need not be kept alive
 * once the constructor returns.
 *
 * \par Experimental
 * This part of the API is experimental, and may change in successive
 * releases without previous notice.
 */
template <BOOST_MYSQL_PIPELINE_STAGE_TYPE... PipelineStageType>
static_pipeline_request<PipelineStageType...> make_pipeline_request(const PipelineStageType&... stages)
{
    return {stages...};
}

#ifndef BOOST_NO_CXX17_DEDUCTION_GUIDES
template <BOOST_MYSQL_PIPELINE_STAGE_TYPE... PipelineStageType>
static_pipeline_request(const PipelineStageType&... args) -> static_pipeline_request<PipelineStageType...>;
#endif

}  // namespace mysql
}  // namespace boost

#include <boost/mysql/impl/pipeline.hpp>
#ifdef BOOST_MYSQL_HEADER_ONLY
#include <boost/mysql/impl/pipeline.ipp>
#endif

#endif
