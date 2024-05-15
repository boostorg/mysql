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
#include <tuple>
#include <vector>

namespace boost {
namespace mysql {

template <class ResultType>
class basic_execute_step
{
    struct impl_t
    {
        detail::pipeline_step_error err_;
        ResultType result_;
        std::uint8_t seqnum;

        void reset_impl(detail::resultset_encoding enc, std::uint8_t s)
        {
            err_.clear();
            detail::access::get_impl(result_).get_interface().reset(enc, metadata_mode::minimal);
            seqnum = s;
        }

        detail::pipeline_step_descriptor to_descriptor()
        {
            return {
                pipeline_step_kind::execute,
                &err_,
                seqnum,
                detail::pipeline_step_descriptor::execute_t{&detail::access::get_impl(result_).get_inteface()}
            };
        }

        void reset(std::vector<std::uint8_t>& buffer, string_view arg)
        {
            reset_impl(detail::resultset_encoding::text, detail::serialize_query(buffer, arg));
        }

        template <class T>
        void reset(std::vector<std::uint8_t>& buffer, const bound_statement_tuple<T>& arg)
        {
            const auto& impl = detail::access::get_impl(arg);

            // Convert to field_view
            auto fields = detail::writable_field_tuple_to_array(impl.params);

            // Serialize the request
            std::uint8_t seqnum = detail::serialize_execute_statement(buffer, impl.stmt, fields);

            reset_impl(detail::resultset_encoding::binary, seqnum);
        }
        // TODO: bound statement range
    } impl_;

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif

public:
    basic_execute_step() = default;
    error_code error() const { return impl_.err_.ec; }
    const diagnostics& diag() const { return impl_.err_.diag; }
    const ResultType& result() const { return impl_.result_; }
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
    struct impl_t
    {
        std::vector<std::uint8_t> buffer_;
        std::tuple<StepType...> steps_;

        detail::pipeline_step_descriptor step_descriptor_at(std::size_t idx)
        {
            return tuple_visitor<sizeof...(StepType)>::invoke(steps_, idx);
        }

        template <std::size_t I, class Arg>
        void reset_step_at(const Arg& arg)
        {
            detail::access::get_impl(std::get<I>(steps_)).reset(buffer_, arg);
        }
    } impl_;

    template <class... Args, std::size_t... I>
    void reset_impl_tuple(std::tuple<const Args&...> args, mp11::index_sequence<I...>)
    {
        int dummy[]{(impl_.reset_step_at<I>(std::get<I>(args)), 0)...};
        ignore_unused(dummy);
    }

    template <class... Args>
    void reset_impl(const Args&... args)
    {
        reset_impl_tuple(std::tuple<const Args&...>{args...}, mp11::make_index_sequence<sizeof...(Args)>{});
    }

public:
    template <class... Args>
    static_pipeline(const Args&... params)
    {
        static_assert(
            sizeof...(Args) == sizeof...(StepType),
            "static_pipeline's constructor requires as many arguments as steps in the pipeline"
        );
        reset_impl(params...);
    }

    template <class... Args>
    void reset(const Args&... params)
    {
        static_assert(
            sizeof...(Args) == sizeof...(StepType),
            "reset requires as many arguments as steps in the pipeline"
        );
        impl_.buffer_.clear();
        reset_impl(params...);
    }

    const std::tuple<StepType...>& steps() const { return impl_.steps_; }
};

}  // namespace mysql
}  // namespace boost

#endif
