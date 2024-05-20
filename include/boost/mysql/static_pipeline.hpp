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
#include <boost/mysql/metadata_mode.hpp>
#include <boost/mysql/pipeline_step_kind.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/statement.hpp>
#include <boost/mysql/static_results.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/any_execution_request.hpp>
#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/pipeline.hpp>
#include <boost/mysql/detail/resultset_encoding.hpp>
#include <boost/mysql/detail/writable_field_traits.hpp>

#include <boost/mysql/impl/statement.hpp>

#include <boost/config/detail/suffix.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/core/span.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/integer_sequence.hpp>
#include <boost/mp11/tuple.hpp>
#include <boost/variant2/variant.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <vector>

namespace boost {
namespace mysql {

class execute_step;
class execute_step_response;

class prepare_statement_step;
class close_statement_step;
class reset_connection_step;
class set_character_set_step;

// Execute
class execute_args
{
    detail::any_execution_request impl_;

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif

public:
    execute_args(string_view query) : impl_(query) {}
    execute_args(statement stmt, span<const field_view> params) : impl_(stmt, params) {}
    using step_type = execute_step;
};

template <std::size_t N>
struct execute_args_store
{
#ifndef BOOST_MYSQL_DOXYGEN
    statement stmt;
    std::array<field_view, N> params;
#endif

    operator execute_args() const { return {stmt, params}; }
    using step_type = execute_step;
};

inline execute_args make_execute_args(string_view v) { return {string_view(v)}; }
inline execute_args make_execute_args(statement stmt, span<const field_view> params)
{
    return {stmt, params};
}

template <class... Args>
execute_args_store<sizeof...(Args)> make_execute_args(statement stmt, const Args&... params)
{
    return {stmt, {detail::to_field(params)...}};
}

class execute_step
{
public:
    using args_type = execute_args;
    using response_type = execute_step_response;

    // TODO: make this private
    static detail::pipeline_request_step create(std::vector<std::uint8_t>& buffer, execute_args args)
    {
        auto args_impl = detail::access::get_impl(args);
        auto enc = args_impl.is_query ? detail::resultset_encoding::text : detail::resultset_encoding::binary;

        std::uint8_t seqnum = args_impl.is_query ? detail::serialize_query(buffer, args_impl.data.query)
                                                 : detail::serialize_execute_statement(
                                                       buffer,
                                                       args_impl.data.stmt.stmt,
                                                       args_impl.data.stmt.params
                                                   );
        return {pipeline_step_kind::execute, seqnum, enc};
    }
};

class execute_step_response
{
    struct impl_t
    {
        variant2::variant<results, detail::err_block> result;

        detail::execution_processor* setup()
        {
            result.emplace<results>();
            return &detail::access::get_impl(variant2::unsafe_get<0>(result));
        }
        void set_result(detail::err_block&& err, statement) { result = std::move(err); }

    } impl_;

    bool has_error() const { return impl_.result.index() == 1u; }

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif

public:
    execute_step_response() = default;
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

// Prepare statement
class prepare_statement_args
{
    string_view impl_;

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif

public:
    prepare_statement_args(string_view stmt_sql) : impl_{stmt_sql} {}

    using step_type = prepare_statement_step;
};

class prepare_statement_step_response
{
    struct impl_t
    {
        variant2::variant<statement, detail::err_block> result;

        detail::execution_processor* setup()
        {
            result.emplace<statement>();
            return nullptr;
        }
        void set_result(detail::err_block&& err, statement stmt)
        {
            if (stmt.valid())
                result = stmt;
            else
                result = std::move(err);
        }

    } impl_;

    bool has_error() const { return impl_.result.index() == 1u; }

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif

public:
    prepare_statement_step_response() = default;
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

class prepare_statement_step
{
public:
    using args_type = prepare_statement_args;
    using response_type = prepare_statement_step_response;

    // TODO: make this private
    static detail::pipeline_request_step create(
        std::vector<std::uint8_t>& buffer,
        prepare_statement_args args
    )
    {
        std::uint8_t seqnum = detail::serialize_prepare_statement(buffer, detail::access::get_impl(args));
        return {pipeline_step_kind::prepare_statement, seqnum, {}};
    }
};

// Generic response type, for steps that don't have a proper result
class no_result_step_response
{
    struct impl_t
    {
        detail::err_block result;

        detail::execution_processor* setup()
        {
            result.ec.clear();
            result.diag.clear();
            return nullptr;
        }
        void set_result(detail::err_block&& err, statement) { result = std::move(err); }

    } impl_;

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif

public:
    no_result_step_response() = default;
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
    using step_type = close_statement_step;
};

class close_statement_step
{
public:
    using args_type = close_statement_args;
    using response_type = no_result_step_response;

    // TODO: make this private
    static detail::pipeline_request_step create(std::vector<std::uint8_t>& buffer, close_statement_args args)
    {
        std::uint8_t seqnum = detail::serialize_close_statement(buffer, detail::access::get_impl(args));
        return {pipeline_step_kind::close_statement, seqnum, {}};
    }
};

// Reset connection
struct reset_connection_args
{
    using step_type = reset_connection_step;
};

class reset_connection_step
{
public:
    using args_type = reset_connection_args;
    using response_type = no_result_step_response;

    // TODO: make this private
    static detail::pipeline_request_step create(std::vector<std::uint8_t>& buffer, reset_connection_args)
    {
        std::uint8_t seqnum = detail::serialize_reset_connection(buffer);
        return {pipeline_step_kind::reset_connection, seqnum, {}};
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
    using step_type = set_character_set_step;
};

class set_character_set_step
{
public:
    using args_type = set_character_set_args;
    using response_type = no_result_step_response;

    // TODO: make this private
    static detail::pipeline_request_step create(
        std::vector<std::uint8_t>& buffer,
        set_character_set_args args
    )
    {
        character_set charset = detail::access::get_impl(args);
        std::uint8_t seqnum = detail::serialize_set_character_set(buffer, charset);
        return {pipeline_step_kind::set_character_set, seqnum, charset};
    }
};

template <class... StepType>
class static_pipeline_request
{
    static_assert(sizeof...(StepType) > 0u, "A pipeline should have one step, at least");

    using step_array_t = std::array<detail::pipeline_request_step, sizeof...(StepType)>;

    struct impl_t
    {
        std::vector<std::uint8_t> buffer_;
        step_array_t steps_;

        impl_t(const StepType::args_type&... args) : steps_{{StepType::create(buffer_, args)...}} {}
    } impl_;

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif

public:
    static_pipeline_request(const typename StepType::args_type&... args) : impl_(args...) {}

    void reset(const typename StepType::args_type&... args)
    {
        impl_.buffer_.clear();
        impl_.steps_ = {StepType::create(impl_.buffer_, args)...};
    }

    using response_type = std::tuple<typename StepType::response_type...>;
};

template <class... Args>
static_pipeline_request<typename Args::step_type...> make_pipeline_request(const Args&... args)
{
    return {args...};
}

template <class... Args>
static_pipeline_request(const Args&... args) -> static_pipeline_request<typename Args::step_type...>;

}  // namespace mysql
}  // namespace boost

// TODO: move to separate header?
// Implementations
namespace boost {
namespace mysql {
namespace detail {

template <std::size_t I>
struct tuple_setup_visitor
{
    template <class... StepType>
    static detail::execution_processor* invoke(std::tuple<StepType...>& t, std::size_t i)
    {
        return i == I - 1 ? detail::access::get_impl(std::get<I - 1>(t)).setup()
                          : tuple_setup_visitor<I - 1>::invoke(t, i);
    }
};

template <>
struct tuple_setup_visitor<0u>
{
    template <class... StepType>
    static detail::execution_processor* invoke(std::tuple<StepType...>&, std::size_t)
    {
        BOOST_ASSERT(false);
        return nullptr;
    }
};
template <std::size_t I>
struct tuple_set_result_visitor
{
    template <class... StepType>
    static void invoke(std::tuple<StepType...>& t, std::size_t i, detail::err_block&& err, statement stmt)
    {
        i == I - 1 ? detail::access::get_impl(std::get<I - 1>(t)).set_result(std::move(err), stmt)
                   : tuple_set_result_visitor<I - 1>::invoke(t, i, std::move(err), stmt);
    }
};

template <>
struct tuple_set_result_visitor<0u>
{
    template <class... StepType>
    static void invoke(std::tuple<StepType...>&, std::size_t, detail::err_block&&, statement)
    {
        BOOST_ASSERT(false);
    }
};

template <class... StepResponseType>
struct pipeline_response_traits<std::tuple<StepResponseType...>>
{
    using response_type = std::tuple<StepResponseType...>;

    static void clear(response_type&) {}

    static execution_processor* setup_step(response_type& self, pipeline_step_kind, std::size_t idx)
    {
        BOOST_ASSERT(idx < sizeof...(StepResponseType));
        return tuple_setup_visitor<sizeof...(StepResponseType)>::invoke(self, idx);
    }

    static void set_step_result(response_type& self, err_block&& err, statement stmt, std::size_t idx)
    {
        BOOST_ASSERT(idx < sizeof...(StepResponseType));
        tuple_set_result_visitor<sizeof...(StepResponseType)>::invoke(self, idx, std::move(err), stmt);
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
