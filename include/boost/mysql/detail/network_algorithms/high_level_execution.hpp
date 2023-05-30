//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_HIGH_LEVEL_EXECUTION_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_HIGH_LEVEL_EXECUTION_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/statement.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/auxiliar/execution_request.hpp>
#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/network_algorithms/execute_impl.hpp>
#include <boost/mysql/detail/network_algorithms/start_execution_impl.hpp>

#include <boost/asio/async_result.hpp>

namespace boost {
namespace mysql {
namespace detail {

class channel;

// Helpers
template <class... T, std::size_t... I>
std::array<field_view, sizeof...(T)> tuple_to_array_impl(const std::tuple<T...>& t, boost::mp11::index_sequence<I...>) noexcept
{
    return std::array<field_view, sizeof...(T)>{{to_field(std::get<I>(t))...}};
}

template <class... T>
std::array<field_view, sizeof...(T)> tuple_to_array(const std::tuple<T...>& t) noexcept
{
    return tuple_to_array_impl(t, boost::mp11::make_index_sequence<sizeof...(T)>());
}

template <class ExecutionRequest, class EnableIf = void>
struct execution_request_getter;

template <>
struct execution_request_getter<string_view>
{
    string_view q;
    execution_request_getter(string_view req) noexcept : q(req) {}
    any_execution_request get() const noexcept { return any_execution_request(q); }
};
inline execution_request_getter<string_view> make_execution_request_getter(string_view req) noexcept
{
    return {req};
}

template <class FieldViewFwdIterator>
struct execution_request_getter<bound_statement_iterator_range<FieldViewFwdIterator>>
{
    statement stmt_;
    std::vector<field_view> params_;  // TODO: this can be optimized
    execution_request_getter(const bound_statement_iterator_range<FieldViewFwdIterator>& req)
        : stmt_(impl_access::get_impl(req).stmt),
          params_(impl_access::get_impl(req).first, impl_access::get_impl(req).last)
    {
    }
    any_execution_request get() const noexcept { return any_execution_request(stmt_, params_); }
};
template <class FieldViewFwdIterator>
execution_request_getter<bound_statement_iterator_range<FieldViewFwdIterator>> make_execution_request_getter(
    const bound_statement_iterator_range<FieldViewFwdIterator>& req
)
{
    return {req};
}

template <class WritableFieldTuple>
struct execution_request_getter<bound_statement_tuple<WritableFieldTuple>>
{
    statement stmt_;
    std::array<field_view, std::tuple_size<WritableFieldTuple>::value> params_;

    execution_request_getter(const bound_statement_tuple<WritableFieldTuple>& req)
        : stmt_(impl_access::get_impl(req).stmt), params_(tuple_to_array(impl_access::get_impl(req).params))
    {
    }
    any_execution_request get() const noexcept { return any_execution_request(stmt_, params_); }
};
template <class WritableFieldTuple>
execution_request_getter<bound_statement_tuple<WritableFieldTuple>> make_execution_request_getter(
    const bound_statement_tuple<WritableFieldTuple>& req
)
{
    return {req};
}

// Async initiations
struct initiate_execute
{
    template <class Handler, BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest>
    void operator()(
        Handler&& handler,
        channel& chan,
        const ExecutionRequest& req,
        execution_processor& proc,
        diagnostics& diag
    )
    {
        auto getter = make_execution_request_getter(req);
        async_execute_impl(chan, getter.get(), proc, diag, std::forward<Handler>(handler));
    }
};

struct initiate_start_execution
{
    template <class Handler, BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest>
    void operator()(
        Handler&& handler,
        channel& chan,
        const ExecutionRequest& req,
        execution_processor& proc,
        diagnostics& diag
    )
    {
        auto getter = make_execution_request_getter(req);
        async_start_execution_impl(chan, getter.get(), proc, diag, std::forward<Handler>(handler));
    }
};

// External interface
template <BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest>
void execute(
    channel& channel,
    const ExecutionRequest& req,
    execution_processor& proc,
    error_code& err,
    diagnostics& diag
)
{
    auto getter = make_execution_request_getter(req);
    execute_impl(channel, getter.get(), proc, err, diag);
}

template <
    BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
async_execute(
    channel& chan,
    ExecutionRequest&& req,
    execution_processor& proc,
    diagnostics& diag,
    CompletionToken&& token
)
{
    return boost::asio::async_initiate<CompletionToken, void(boost::mysql::error_code)>(
        initiate_execute(),
        token,
        std::ref(chan),
        std::forward<ExecutionRequest>(req),
        std::ref(proc),
        std::ref(diag)
    );
}

template <BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest>
void start_execution(
    channel& channel,
    const ExecutionRequest& req,
    execution_processor& proc,
    error_code& err,
    diagnostics& diag
)
{
    auto getter = make_execution_request_getter(req);
    start_execution_impl(channel, getter.get(), proc, err, diag);
}

template <
    BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
async_start_execution(
    channel& chan,
    ExecutionRequest&& req,
    execution_processor& proc,
    diagnostics& diag,
    CompletionToken&& token
)
{
    return boost::asio::async_initiate<CompletionToken, void(boost::mysql::error_code)>(
        initiate_start_execution(),
        token,
        std::ref(chan),
        std::forward<ExecutionRequest>(req),
        std::ref(proc),
        std::ref(diag)
    );
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
