//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_PIPELINE_HPP
#define BOOST_MYSQL_IMPL_PIPELINE_HPP

#pragma once

#include <boost/mysql/pipeline.hpp>

#include <cstddef>
#include <type_traits>

namespace boost {
namespace mysql {
namespace detail {

// Stage concepts
template <>
struct is_pipeline_stage_type<execute_stage> : std::true_type
{
};

template <>
struct is_pipeline_stage_type<prepare_statement_stage> : std::true_type
{
};

template <>
struct is_pipeline_stage_type<close_statement_stage> : std::true_type
{
};

template <>
struct is_pipeline_stage_type<reset_connection_stage> : std::true_type
{
};

template <>
struct is_pipeline_stage_type<set_character_set_stage> : std::true_type
{
};

// Request concepts
template <>
struct is_pipeline_request_type<pipeline_request> : std::true_type
{
};

template <class... PipelineStageType>
struct is_pipeline_request_type<static_pipeline_request<PipelineStageType...>> : std::true_type
{
};

// Response traits
template <>
struct pipeline_response_traits<std::vector<any_stage_response>>
{
    using response_type = std::vector<any_stage_response>;

    static void setup(response_type& self, span<const pipeline_request_stage> request)
    {
        // Create as many response items as request stages
        self.resize(request.size());

        // Setup them
        for (std::size_t i = 0u; i < request.size(); ++i)
        {
            // Execution stages need to be initialized to results objects.
            // Otherwise, clear any previous content
            auto& impl = access::get_impl(self[i]);
            if (request[i].kind == pipeline_stage_kind::execute)
                impl.emplace_results();
            else
                impl.emplace_error();
        }
    }

    static execution_processor& get_processor(response_type& self, std::size_t idx)
    {
        BOOST_ASSERT(idx < self.size());
        return access::get_impl(self[idx]).get_processor();
    }

    static void set_result(response_type& self, std::size_t idx, statement stmt)
    {
        BOOST_ASSERT(idx < self.size());
        access::get_impl(self[idx]).set_result(stmt);
    }

    static void set_error(response_type& self, std::size_t idx, error_code ec, diagnostics&& diag)
    {
        BOOST_ASSERT(idx < self.size());
        access::get_impl(self[idx]).set_error(ec, std::move(diag));
    }
};

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
