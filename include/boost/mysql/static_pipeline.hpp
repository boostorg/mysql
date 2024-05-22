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

#include <boost/core/ignore_unused.hpp>
#include <boost/core/span.hpp>
#include <boost/mp11/tuple.hpp>
#include <boost/variant2/variant.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <tuple>
#include <utility>
#include <vector>

namespace boost {
namespace mysql {

class execute_stage;
class prepare_statement_stage;
class close_statement_stage;
class reset_connection_stage;
class set_character_set_stage;
class writable_field_arg;

// TODO: move this
// TODO: could we unify this and any_execution_request?
namespace detail {

struct execute_args_impl
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

    type_t type;
    data_t data;

    execute_args_impl(string_view q) noexcept : type(type_query), data(q) {}
    execute_args_impl(statement s, span<const writable_field_arg> params) noexcept
        : type(type_stmt_tuple), data(s, params)
    {
    }
    execute_args_impl(statement s, span<const field_view> params) noexcept
        : type(type_stmt_range), data(s, params)
    {
    }
};

}  // namespace detail

// Execute
class writable_field_arg
{
    field_view impl_;
#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif
public:
    template <class WritableField>  // TODO: concept
    writable_field_arg(const WritableField& f) noexcept : impl_(detail::to_field(f))
    {
    }
};

class execute_stage_response
{
    struct impl_t
    {
        variant2::variant<results, detail::err_block> result;

        void reset() { result.emplace<results>(); }
        detail::execution_processor* get_processor()
        {
            return &detail::access::get_impl(variant2::unsafe_get<0>(result));
        }
        void set_result(statement) { BOOST_ASSERT(false); }
        void set_error(detail::err_block&& err) { result = std::move(err); }

    } impl_;

    bool has_error() const { return impl_.result.index() == 1u; }

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif

public:
    execute_stage_response() = default;
    error_code error() const { return has_error() ? variant2::unsafe_get<1>(impl_.result).ec : error_code(); }
    diagnostics diag() const&
    {
        return has_error() ? variant2::unsafe_get<1>(impl_.result).diag : diagnostics();
    }
    diagnostics diag() &&
    {
        return has_error() ? variant2::unsafe_get<1>(std::move(impl_.result)).diag : diagnostics();
    }
    const results& result() const& { return variant2::unsafe_get<0>(impl_.result); }
    results&& result() && { return variant2::unsafe_get<0>(std::move(impl_.result)); }
};

class execute_args
{
    detail::execute_args_impl impl_;

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif

public:
    execute_args(string_view query) : impl_(query) {}
    execute_args(statement stmt, span<const field_view> params) : impl_(stmt, params) {}
    execute_args(statement stmt, std::initializer_list<writable_field_arg> params) : impl_(stmt, params) {}
    using stage_type = execute_stage;
};

class execute_stage
{
public:
    using args_type = execute_args;
    using response_type = execute_stage_response;

    // TODO: make this private
    // TODO: move to compiled
    static detail::pipeline_request_stage create(std::vector<std::uint8_t>& buffer, execute_args args)
    {
        auto args_impl = detail::access::get_impl(args);
        switch (args_impl.type)
        {
        case detail::execute_args_impl::type_query:
            return detail::serialize_query(buffer, args_impl.data.query);
        case detail::execute_args_impl::type_stmt_tuple:
        {
            constexpr std::size_t stack_fields = 64u;
            auto params = args_impl.data.stmt_tuple.params;
            auto stmt = args_impl.data.stmt_tuple.stmt;
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
        case detail::execute_args_impl::type_stmt_range:
            return detail::serialize_execute_statement(
                buffer,
                args_impl.data.stmt_range.stmt,
                args_impl.data.stmt_range.params
            );
        }
    }
};

// Prepare statement
class prepare_statement_args
{
    string_view impl_;

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif

public:
    prepare_statement_args(string_view stmt_sql) : impl_{stmt_sql} {}

    using stage_type = prepare_statement_stage;
};

class prepare_statement_stage_response
{
    struct impl_t
    {
        variant2::variant<statement, detail::err_block> result;

        void reset() { result.emplace<statement>(); }
        detail::execution_processor* get_processor()
        {
            BOOST_ASSERT(false);
            return nullptr;
        }
        void set_result(statement stmt) { result = stmt; }
        void set_error(detail::err_block&& err) { result = std::move(err); }

    } impl_;

    bool has_error() const { return impl_.result.index() == 1u; }

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif

public:
    prepare_statement_stage_response() = default;
    error_code error() const { return has_error() ? variant2::unsafe_get<1>(impl_.result).ec : error_code(); }
    diagnostics diag() const&
    {
        return has_error() ? variant2::unsafe_get<1>(impl_.result).diag : diagnostics();
    }
    diagnostics diag() &&
    {
        return has_error() ? variant2::unsafe_get<1>(std::move(impl_.result)).diag : diagnostics();
    }
    statement result() const { return variant2::unsafe_get<0>(impl_.result); }
};

class prepare_statement_stage
{
public:
    using args_type = prepare_statement_args;
    using response_type = prepare_statement_stage_response;

    // TODO: make this private
    static detail::pipeline_request_stage create(
        std::vector<std::uint8_t>& buffer,
        prepare_statement_args args
    )
    {
        return detail::serialize_prepare_statement(buffer, detail::access::get_impl(args));
    }
};

// Generic response type, for stages that don't have a proper result
class no_result_stage_response
{
    struct impl_t
    {
        detail::err_block result;

        void reset()
        {
            result.ec.clear();
            result.diag.clear();
        }
        detail::execution_processor* get_processor()
        {
            BOOST_ASSERT(false);
            return nullptr;
        }
        void set_result(statement) { BOOST_ASSERT(false); }
        void set_error(detail::err_block&& err) { result = std::move(err); }

    } impl_;

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif

public:
    no_result_stage_response() = default;
    error_code error() const { return impl_.result.ec; }
    const diagnostics& diag() const& { return impl_.result.diag; }
    diagnostics&& diag() && { return std::move(impl_.result.diag); }
};

// Close statement
class close_statement_args
{
    statement impl_;
#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif

public:
    close_statement_args(statement stmt) : impl_(stmt) {}
    using stage_type = close_statement_stage;
};

class close_statement_stage
{
public:
    using args_type = close_statement_args;
    using response_type = no_result_stage_response;

    // TODO: make this private
    static detail::pipeline_request_stage create(std::vector<std::uint8_t>& buffer, close_statement_args args)
    {
        return detail::serialize_close_statement(buffer, detail::access::get_impl(args).id());
    }
};

// Reset connection
struct reset_connection_args
{
    using stage_type = reset_connection_stage;
};

class reset_connection_stage
{
public:
    using args_type = reset_connection_args;
    using response_type = no_result_stage_response;

    // TODO: make this private
    static detail::pipeline_request_stage create(std::vector<std::uint8_t>& buffer, reset_connection_args)
    {
        return detail::serialize_reset_connection(buffer);
    }
};

// Set character set
class set_character_set_args
{
    character_set impl_;
#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif

public:
    set_character_set_args(character_set charset) : impl_(charset) {}
    using stage_type = set_character_set_stage;
};

class set_character_set_stage
{
public:
    using args_type = set_character_set_args;
    using response_type = no_result_stage_response;

    // TODO: make this private
    static detail::pipeline_request_stage create(
        std::vector<std::uint8_t>& buffer,
        set_character_set_args args
    )
    {
        return detail::serialize_set_character_set(buffer, detail::access::get_impl(args));
    }
};

template <class... StageType>
class static_pipeline_request
{
    static_assert(sizeof...(StageType) > 0u, "A pipeline should have one stage, at least");

    using stage_array_t = std::array<detail::pipeline_request_stage, sizeof...(StageType)>;

    struct impl_t
    {
        std::vector<std::uint8_t> buffer_;
        stage_array_t stages_;

        impl_t(const StageType::args_type&... args) : stages_{{StageType::create(buffer_, args)...}} {}
    } impl_;

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif

public:
    static_pipeline_request(const typename StageType::args_type&... args) : impl_(args...) {}

    void reset(const typename StageType::args_type&... args)
    {
        impl_.buffer_.clear();
        impl_.stages_ = {StageType::create(impl_.buffer_, args)...};
    }

    using response_type = std::tuple<typename StageType::response_type...>;
};

template <class... Args>
static_pipeline_request<typename Args::stage_type...> make_pipeline_request(const Args&... args)
{
    return {args...};
}

template <class... Args>
static_pipeline_request(const Args&... args) -> static_pipeline_request<typename Args::stage_type...>;

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
    template <class StageType>
    void operator()(StageType& stage) const
    {
        access::get_impl(stage).reset();
    }
};

struct stage_get_processor_visitor
{
    template <class StageType>
    execution_processor* operator()(StageType& stage) const
    {
        return access::get_impl(stage).get_processor();
    }
};

struct stage_set_result_visitor
{
    template <class StageType>
    void operator()(StageType& stage, statement stmt) const
    {
        return access::get_impl(stage).set_result(stmt);
    }
};

struct stage_set_error_visitor
{
    template <class StageType>
    void operator()(StageType& stage, err_block&& err) const
    {
        return access::get_impl(stage).set_error(std::move(err));
    }
};

template <class... StageResponseType>
struct pipeline_response_traits<std::tuple<StageResponseType...>>
{
    using response_type = std::tuple<StageResponseType...>;

    static void setup(response_type& self, span<const pipeline_request_stage> request)
    {
        BOOST_ASSERT(request.size() == sizeof...(StageResponseType));
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

    static void set_error(response_type& self, std::size_t idx, err_block&& err)
    {
        tuple_index(self, idx, stage_set_error_visitor{}, std::move(err));
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
