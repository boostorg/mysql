//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_STATIC_PIPELINE_HPP
#define BOOST_MYSQL_STATIC_PIPELINE_HPP

#include <boost/mysql/field_view.hpp>
#include <boost/mysql/metadata_mode.hpp>
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
#include <boost/core/ignore_unused.hpp>
#include <boost/core/span.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/integer_sequence.hpp>
#include <boost/mp11/tuple.hpp>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <tuple>
#include <type_traits>
#include <vector>

namespace boost {
namespace mysql {

struct writable_field_ref
{
    field_view fv;

    template <class T, class = typename std::enable_if<detail::is_writable_field<T>::value>::type>
    writable_field_ref(const T& t) noexcept : fv(detail::to_field(t))
    {
    }
};

// TODO: hide this and unify with any_execution_request
struct execute_step_args_impl
{
    enum class type_t
    {
        query,
        stmt_tuple,
        stmt_range
    };

    struct stmt_tuple_t
    {
        statement stmt;
        span<const field_view> params;
    };

    struct stmt_range_t
    {
        statement stmt;
        std::initializer_list<writable_field_ref> params;
    };

    union data_t
    {
        string_view query;
        stmt_tuple_t stmt_tuple;
        stmt_range_t stmt_range;

        data_t(string_view v) : query(v) {}
        data_t(stmt_tuple_t v) : stmt_tuple(v) {}
        data_t(stmt_range_t v) : stmt_range(v) {}
    };

    type_t type;
    data_t data;
};

class execute_step_args
{
    execute_step_args_impl impl_;

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif

public:
    template <class T, class = std::enable_if<std::is_convertible<T, string_view>::value>::type>
    execute_step_args(const T& query) : impl_{execute_step_args_impl::type_t::query, query}
    {
    }

    execute_step_args(statement stmt, std::initializer_list<writable_field_ref> params)
        : impl_{execute_step_args_impl::type_t::stmt_tuple, {{stmt, params}}}
    {
    }
    execute_step_args(statement stmt, span<const field_view> params)
        : impl_{execute_step_args_impl::type_t::stmt_range, {{stmt, params}}}
    {
    }
};

BOOST_MYSQL_DECL std::uint8_t serialize_execute(
    std::vector<std::uint8_t>& buffer,
    execute_step_args_impl args
);

template <class ResultType>
class basic_execute_step
{
    struct impl_t
    {
        detail::pipeline_step_error err_;
        ResultType result_;
        std::uint8_t seqnum;

        detail::pipeline_step_descriptor to_descriptor()
        {
            return {
                pipeline_step_kind::execute,
                &err_,
                seqnum,
                detail::pipeline_step_descriptor::execute_t{&detail::access::get_impl(result_).get_inteface()}
            };
        }

        void reset(std::vector<std::uint8_t>& buffer, execute_step_args args)
        {
            auto args_impl = detail::access::get_impl(args);
            auto enc = args_impl.type == execute_step_args_impl::type_t::query
                           ? detail::resultset_encoding::text
                           : detail::resultset_encoding::binary;

            seqnum = serialize_execute(buffer, args_impl);
            err_.clear();
            detail::access::get_impl(result_).get_interface().reset(enc, metadata_mode::minimal);
        }
    } impl_;

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif

public:
    basic_execute_step() = default;
    error_code error() const { return impl_.err_.ec; }
    const diagnostics& diag() const { return impl_.err_.diag; }
    const ResultType& result() const { return impl_.result_; }

    using args_type = execute_step_args;
};

using execute_step = basic_execute_step<results>;

template <class... StaticRow>
using static_execute_step = basic_execute_step<static_results<StaticRow...>>;

class prepare_statement_step
{
    struct
    {
        detail::pipeline_step_error err_;
        statement result_;
        std::uint8_t seqnum;

        detail::pipeline_step_descriptor to_descriptor()
        {
            return {
                pipeline_step_kind::prepare_statement,
                &err_,
                seqnum,
                detail::pipeline_step_descriptor::prepare_statement_t{&result_}
            };
        }

        void reset(std::vector<std::uint8_t>& buffer, string_view stmt_sql)
        {
            err_.clear();
            result_ = statement();
            seqnum = detail::serialize_prepare_statement(buffer, stmt_sql);
        }
    } impl_;

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif

public:
    prepare_statement_step() = default;
    error_code error() const { return impl_.err_.ec; }
    const diagnostics& diag() const { return impl_.err_.diag; }
    statement result() const { return impl_.result_; }

    using args_type = string_view;
};

class close_statement_step
{
    struct
    {
        detail::pipeline_step_error err_;
        std::uint8_t seqnum;

        detail::pipeline_step_descriptor to_descriptor()
        {
            return {
                pipeline_step_kind::close_statement,
                &err_,
                seqnum,
                detail::pipeline_step_descriptor::step_specific_t{}
            };
        }

        void reset(std::vector<std::uint8_t>& buffer, statement stmt)
        {
            err_.clear();
            seqnum = detail::serialize_close_statement(buffer, stmt);
        }
    } impl_;

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif

public:
    close_statement_step() = default;
    error_code error() const { return impl_.err_.ec; }
    const diagnostics& diag() const { return impl_.err_.diag; }

    using args_type = statement;
};

struct no_arg_t
{
};

BOOST_INLINE_CONSTEXPR no_arg_t no_arg{};

class reset_connection_step
{
    struct
    {
        detail::pipeline_step_error err_;
        std::uint8_t seqnum;

        detail::pipeline_step_descriptor to_descriptor()
        {
            return {
                pipeline_step_kind::reset_connection,
                &err_,
                seqnum,
                detail::pipeline_step_descriptor::step_specific_t{}
            };
        }

        void reset(std::vector<std::uint8_t>& buffer, no_arg_t)
        {
            err_.clear();
            seqnum = detail::serialize_reset_connection(buffer);
        }
    } impl_;

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif

public:
    reset_connection_step() = default;
    error_code error() const { return impl_.err_.ec; }
    const diagnostics& diag() const { return impl_.err_.diag; }

    using args_type = no_arg_t;
};

// TODO: hide these
template <std::size_t I>
struct tuple_visitor
{
    template <class... StepType>
    detail::pipeline_step_descriptor invoke(std::tuple<StepType...>& t, std::size_t i)
    {
        return i == I - 1 ? detail::access::get_impl(std::get<I - 1>(t)).to_descriptor()
                          : tuple_visitor<I - 2>::invoke(t, i);
    }
};

template <>
struct tuple_visitor<0u>
{
    template <class... StepType>
    detail::pipeline_step_descriptor invoke(std::tuple<StepType...>&, std::size_t)
    {
        return detail::pipeline_step_descriptor{};
    }
};

template <class... StepType>
class static_pipeline
{
    static_assert(sizeof...(StepType) > 0u, "A pipeline should have one step, at least");

    struct impl_t
    {
        std::vector<std::uint8_t> buffer_;
        std::tuple<StepType...> steps_;

        detail::pipeline_step_descriptor step_descriptor_at(std::size_t idx)
        {
            return tuple_visitor<sizeof...(StepType)>::invoke(steps_, idx);
        }

    } impl_;

    template <std::size_t... I>
    void reset_impl(mp11::index_sequence<I...>, const typename StepType::args_type&... args)
    {
        int dummy[]{(detail::access::get_impl(std::get<I>(impl_.steps_)).reset(impl_.buffer_, args), 0)...};
        ignore_unused(dummy);
    }

    void reset_impl(const typename StepType::args_type&... args)
    {
        reset_impl(mp11::make_index_sequence<sizeof...(StepType)>{}, args...);
    }

public:
    static_pipeline(const typename StepType::args_type&... args) { reset_impl(args...); }

    void reset(const typename StepType::args_type&... args)
    {
        impl_.buffer_.clear();
        reset_impl(args...);
    }

    const std::tuple<StepType...>& steps() const { return impl_.steps_; }
};

}  // namespace mysql
}  // namespace boost

#endif
