//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_STATIC_PIPELINE_HPP
#define BOOST_MYSQL_STATIC_PIPELINE_HPP

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
#include <boost/mysql/detail/resultset_encoding.hpp>
#include <boost/mysql/detail/writable_field_traits.hpp>

#include <boost/core/ignore_unused.hpp>
#include <boost/core/span.hpp>
#include <boost/mp11/tuple.hpp>
#include <boost/mp11/utility.hpp>
#include <boost/system/result.hpp>
#include <boost/throw_exception.hpp>
#include <boost/variant2/variant.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace boost {
namespace mysql {

// Execute
class writable_field_arg
{
    field_view impl_;
#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif
public:
    template <BOOST_MYSQL_WRITABLE_FIELD WritableField>
    writable_field_arg(const WritableField& f) noexcept : impl_(detail::to_field(f))
    {
    }
};

class execute_stage
{
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
            span<const writable_field_arg> params;
        } stmt_tuple;
        struct
        {
            statement stmt;
            span<const field_view> params;
        } stmt_range;

        data_t(string_view q) noexcept : query(q) {}
        data_t(statement s, span<const writable_field_arg> params) noexcept : stmt_tuple{s, params} {}
        data_t(statement s, span<const field_view> params) noexcept : stmt_range{s, params} {}
    };

    type_t type_;
    data_t data_;

    // TODO: move to compiled
    detail::pipeline_request_stage create(std::vector<std::uint8_t>& buffer) const
    {
        switch (type_)
        {
        case type_query: return detail::serialize_query(buffer, data_.query);
        case type_stmt_tuple:
        {
            constexpr std::size_t stack_fields = 64u;
            auto params = data_.stmt_tuple.params;
            auto stmt = data_.stmt_tuple.stmt;
            if (params.size() <= stack_fields)
            {
                std::array<field_view, stack_fields> storage;
                for (std::size_t i = 0; i < params.size(); ++i)
                    storage[i] = detail::access::get_impl(params[i]);
                return detail::serialize_execute_statement(buffer, stmt, {storage.data(), params.size()});
            }
            else
            {
                std::vector<field_view> storage;
                storage.reserve(params.size());
                for (auto p : params)
                    storage.push_back(detail::access::get_impl(p));
                return detail::serialize_execute_statement(buffer, stmt, storage);
            }
        }
        case type_stmt_range:
            return detail::serialize_execute_statement(
                buffer,
                data_.stmt_range.stmt,
                data_.stmt_range.params
            );
        }
    }

#ifndef BOOST_MYSQL_DOXYGEN
    template <class... PipelineStageType>
    friend class static_pipeline_request;
#endif

public:
    execute_stage(string_view query) : type_(type_query), data_(query) {}
    execute_stage(statement stmt, std::initializer_list<writable_field_arg> params)
        : type_(type_stmt_tuple), data_(stmt, {params.begin(), params.size()})
    {
        // TODO: this should throw on invalid params
    }
    execute_stage(statement stmt, span<const field_view> params) : type_(type_stmt_range), data_(stmt, params)
    {
    }
    using response_type = system::result<results, errcode_with_diagnostics>;
};

// Prepare statement
class prepare_statement_stage
{
    string_view stmt_sql_;

    detail::pipeline_request_stage create(std::vector<std::uint8_t>& buffer) const
    {
        return detail::serialize_prepare_statement(buffer, stmt_sql_);
    }

#ifndef BOOST_MYSQL_DOXYGEN
    template <class... PipelineStageType>
    friend class static_pipeline_request;
#endif

public:
    prepare_statement_stage(string_view stmt_sql) : stmt_sql_(stmt_sql) {}

    using response_type = system::result<statement, errcode_with_diagnostics>;
};

// Close statement
class close_statement_stage
{
    std::uint32_t stmt_id_;

    detail::pipeline_request_stage create(std::vector<std::uint8_t>& buffer) const
    {
        return detail::serialize_close_statement(buffer, stmt_id_);
    }

#ifndef BOOST_MYSQL_DOXYGEN
    template <class... PipelineStageType>
    friend class static_pipeline_request;
#endif

public:
    close_statement_stage(statement stmt) : stmt_id_(stmt.id()) {}
    using response_type = errcode_with_diagnostics;
};

// Reset connection
class reset_connection_stage
{
    detail::pipeline_request_stage create(std::vector<std::uint8_t>& buffer) const
    {
        return detail::serialize_reset_connection(buffer);
    }

#ifndef BOOST_MYSQL_DOXYGEN
    template <class... PipelineStageType>
    friend class static_pipeline_request;
#endif

public:
    reset_connection_stage() = default;
    using response_type = errcode_with_diagnostics;
};

// Set character set
class set_character_set_stage
{
    character_set charset_;

    detail::pipeline_request_stage create(std::vector<std::uint8_t>& buffer) const
    {
        return detail::serialize_set_character_set(buffer, charset_);
    }

#ifndef BOOST_MYSQL_DOXYGEN
    template <class... PipelineStageType>
    friend class static_pipeline_request;
#endif

public:
    set_character_set_stage(character_set charset) : charset_(charset) {}
    using response_type = errcode_with_diagnostics;
};

// TODO: move this
namespace detail {

template <class T>
constexpr bool is_stage_type()
{
    return std::is_same<T, execute_stage>::value || std::is_same<T, prepare_statement_stage>::value ||
           std::is_same<T, close_statement_stage>::value || std::is_same<T, reset_connection_stage>::value ||
           std::is_same<T, set_character_set_stage>::value;
}

constexpr bool check_stage_types() { return true; }

template <class T1, class... Rest>
constexpr bool check_stage_types(mp11::mp_identity<T1>, mp11::mp_identity<Rest>... rest)
{
    static_assert(
        is_stage_type<T1>(),
        "static_pipeline_request instantiated with an invalid stage type. Valid stage types: execute_stage, "
        "prepare_statement_stage, close_statement_stage, reset_connection_stage, set_character_set_stage"
    );
    return check_stage_types(rest...);
}

}  // namespace detail

/**
 * \brief A static pipeline request type.
 * \details
 * Contains a collection of pipeline stages, fully describing the work to be performed
 * by a pipeline operation. The pipeline contains as many stages as `PipelineStageType`
 * template parameters are passed to this type.
 * \n
 * The following types can be used as `PipelineStageType` template parameters: \n
 *    \li \ref execute_stage: behavior equivalent to \ref any_connection::execute
 *    \li \ref prepare_statement_stage: behavior equivalent to \ref any_connection::prepare_statement
 *    \li \ref close_statement_stage: behavior equivalent to \ref any_connection::close_statement
 *    \li \ref reset_connection_stage: behavior equivalent to \ref any_connection::reset_connection
 *    \li \ref set_character_set_stage: behavior equivalent to \ref any_connection::set_character_set
 * \n
 * Stage responses are read into `std::tuple` objects, with individual elements depending
 * on stage types. See \ref response_type for more info.
 * \n
 * To create instances of this class without specifying template parameters,
 * you can use \ref make_pipeline_request. If you're on C++17 or above, appropriate class deduction
 * guidelines are also provided.
 */
template <class... PipelineStageType>
class static_pipeline_request
{
    static_assert(sizeof...(PipelineStageType) > 0u, "A pipeline should have one stage, at least");
    static_assert(detail::check_stage_types(mp11::mp_identity<PipelineStageType>{}...), "");

    using stage_array_t = std::array<detail::pipeline_request_stage, sizeof...(PipelineStageType)>;

    static stage_array_t create_stage_array(std::vector<std::uint8_t>& buff, const PipelineStageType&... args)
    {
        return {{args.create(buff)...}};
    }

    struct impl_t
    {
        std::vector<std::uint8_t> buffer_;
        stage_array_t stages_;

        impl_t(const PipelineStageType&... args) : stages_(create_stage_array(buffer_, args...)) {}
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
     *
     * \par Object lifetimes
     * Parameters are stored as a serialized request on the constructed object.
     * The `stages` objects, and any other objects they point to, need not be kept alive
     * once the constructor returns.
     */
    static_pipeline_request(const PipelineStageType&... stages) : impl_(stages...) {}

    /**
     * \brief Replaces the request with a new one containing the supplied stages.
     * \details
     * The effect is equivalent to `*this = static_pipeline_request(stages)`, but
     * may be more efficient.
     * \n
     * The supplied `stages` must have the same type as the current ones.
     *
     * \par Exception safety
     * Basic guarantee. Memory allocations while composing the request may throw.
     *
     * \par Object lifetimes
     * The `stages` objects, and any other objects they point to, need not be kept alive
     * once this function returns.
     */
    void assign(const PipelineStageType&... stages)
    {
        impl_.buffer_.clear();
        impl_.stages_ = create_stage_array(impl_.buffer_, stages...);
    }

    /**
     * \brief The response type to use when running a pipeline with this request type.
     * \details
     * Has the same number of elements as pipeline stages. Consult each stage
     * `response_type` typedef for more info.
     */
    using response_type = std::tuple<typename PipelineStageType::response_type...>;
};

/**
 * \brief Creates a static_pipeline_request object.
 * \details
 * This factory function can be used to avoid having to explicitly specify
 * template parameters on the request type. For C++17 code and later,
 * you can directly use \ref static_pipeline_request constructor, instead.
 *
 * \par Exception safety
 * Strong guarantee. Memory allocations while composing the request may throw.
 *
 * \par Object lifetimes
 * The `stages` objects, and any other objects they point to, need not be kept alive
 * once the constructor returns.
 */
template <class... PipelineStageType>
static_pipeline_request<PipelineStageType...> make_pipeline_request(const PipelineStageType&... stages)
{
    return {stages...};
}

template <class... PipelineStageType>
static_pipeline_request(const PipelineStageType&... args) -> static_pipeline_request<PipelineStageType...>;

}  // namespace mysql
}  // namespace boost

// TODO: move to separate header?
// Implementations
namespace boost {
namespace mysql {
namespace detail {

// Index a tuple at runtime
template <std::size_t I, class R>
struct tuple_index_visitor
{
    template <class... T, class Fn, class... Args>
    static R invoke(std::tuple<T...>& t, std::size_t i, Fn f, Args&&... args)
    {
        return i == I ? f(std::get<I>(t), std::forward<Args>(args)...)
                      : tuple_index_visitor<I - 1, R>::invoke(t, i, f, std::forward<Args>(args)...);
    }
};

template <class R>
struct tuple_index_visitor<0u, R>
{
    template <class... T, class Fn, class... Args>
    static R invoke(std::tuple<T...>& t, std::size_t i, Fn f, Args&&... args)
    {
        BOOST_ASSERT(i == 0u);
        ignore_unused(i);
        return f(std::get<0u>(t), std::forward<Args>(args)...);
    }
};

template <class... T, class Fn, class... Args>
auto tuple_index(std::tuple<T...>& t, std::size_t i, Fn f, Args&&... args)
    -> decltype(f(std::get<0u>(t), std::forward<Args>(args)...))
{
    static_assert(sizeof...(T) > 0u, "Empty tuples not allowed");
    BOOST_ASSERT(i < sizeof...(T));
    using result_type = decltype(f(std::get<0u>(t), std::forward<Args>(args)...));
    return tuple_index_visitor<sizeof...(T) - 1, result_type>::invoke(t, i, f, std::forward<Args>(args)...);
}

// Concrete visitors
struct stage_reset_visitor
{
    void operator()(execute_stage::response_type& value) const { value.emplace(); }
    void operator()(prepare_statement_stage::response_type& value) const { value.emplace(); }

    void operator()(errcode_with_diagnostics& value) const
    {
        value.code.clear();
        value.diag.clear();
    }
};

struct stage_get_processor_visitor
{
    execution_processor* operator()(execute_stage::response_type& value) const
    {
        return &access::get_impl(*value);
    }

    template <class PipelineStageType>
    execution_processor* operator()(PipelineStageType&) const
    {
        BOOST_ASSERT(false);
        return nullptr;
    }
};

struct stage_set_result_visitor
{
    void operator()(prepare_statement_stage::response_type& r, statement s) const { r = s; }

    template <class PipelineStageType>
    void operator()(PipelineStageType&, statement) const
    {
        BOOST_ASSERT(false);
    }
};

struct stage_set_error_visitor
{
    template <class PipelineStageType>
    void operator()(PipelineStageType& r, error_code ec, diagnostics&& diag) const
    {
        r = errcode_with_diagnostics{ec, std::move(diag)};
    }
};

template <class... PipelineStageResponseType>
struct pipeline_response_traits<std::tuple<PipelineStageResponseType...>>
{
    using response_type = std::tuple<PipelineStageResponseType...>;

    static void setup(response_type& self, span<const pipeline_request_stage> request)
    {
        BOOST_ASSERT(request.size() == sizeof...(PipelineStageResponseType));
        ignore_unused(request);
        mp11::tuple_for_each(self, stage_reset_visitor{});
    }

    static execution_processor& get_processor(response_type& self, std::size_t idx)
    {
        return *tuple_index(self, idx, stage_get_processor_visitor{});
    }

    static void set_result(response_type& self, std::size_t idx, statement stmt)
    {
        tuple_index(self, idx, stage_set_result_visitor{}, stmt);
    }

    static void set_error(response_type& self, std::size_t idx, error_code ec, diagnostics&& diag)
    {
        tuple_index(self, idx, stage_set_error_visitor{}, ec, std::move(diag));
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
