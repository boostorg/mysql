//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_HIGH_LEVEL_EXECUTION_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_HIGH_LEVEL_EXECUTION_HPP

#pragma once

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/auxiliar/execution_request.hpp>
#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/network_algorithms/execute_impl.hpp>
#include <boost/mysql/detail/network_algorithms/high_level_execution.hpp>
#include <boost/mysql/detail/network_algorithms/start_execution_impl.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>
#include <boost/mysql/detail/typing/writable_field_traits.hpp>

#include <boost/mp11/integer_sequence.hpp>

#include <cstddef>
#include <tuple>

namespace boost {
namespace mysql {
namespace detail {

class channel;

// Helpers
BOOST_MYSQL_DECL
void serialize_execution_request(string_view sql, channel& to);

BOOST_MYSQL_DECL
void serialize_execution_request_impl(std::uint32_t stmt_id, span<const field_view> params, channel& to);

// Text queries
inline resultset_encoding get_encoding(string_view) noexcept { return resultset_encoding::text; }

inline error_code check_client_errors(string_view) noexcept { return error_code(); }

// Helpers for statements
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

inline error_code check_num_params(const statement& stmt, std::size_t param_count)
{
    return param_count == stmt.num_params() ? error_code() : make_error_code(client_errc::wrong_num_params);
}

// Statement, tuple
template <class WritableFieldTuple>
resultset_encoding get_encoding(const bound_statement_tuple<WritableFieldTuple>&)
{
    return resultset_encoding::binary;
}

template <class WritableFieldTuple>
void serialize_execution_request(const bound_statement_tuple<WritableFieldTuple>& req, channel& chan)
{
    const auto& impl = impl_access::get_impl(req);
    auto arr = tuple_to_array(impl.params);
    serialize_execution_request_impl(impl.stmt.id(), arr, chan);
}

template <class WritableFieldTuple>
error_code check_client_errors(const bound_statement_tuple<WritableFieldTuple>& req)
{
    return check_num_params(impl_access::get_impl(req).stmt, std::tuple_size<WritableFieldTuple>::value);
}

// Statement, iterators
template <class FieldViewFwdIterator>
resultset_encoding get_encoding(const bound_statement_iterator_range<FieldViewFwdIterator>&)
{
    return resultset_encoding::binary;
}

template <class FieldViewFwdIterator>
void serialize_execution_request(
    const bound_statement_iterator_range<FieldViewFwdIterator>& req,
    channel& chan
)
{
    const auto& impl = impl_access::get_impl(req);
    // TODO: this can be optimized
    std::vector<field_view> fields(impl.first, impl.last);
    serialize_execution_request_impl(impl.stmt.id(), fields, chan);
}

template <class FieldViewFwdIterator>
error_code check_client_errors(const bound_statement_iterator_range<FieldViewFwdIterator>& req)
{
    const auto& impl = impl_access::get_impl(req);
    return check_num_params(impl.stmt, std::distance(impl.first, impl.last));
}

// Async initiations
struct initiate_execute
{
    template <class Handler, BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest>
    void operator()(
        Handler&& handler,
        channel& chan,
        const ExecutionRequest& req,
        execution_processor& result,
        diagnostics& diag
    )
    {
        serialize_execution_request(req, chan);
        async_execute_impl(
            chan,
            check_client_errors(req),
            get_encoding(req),
            result,
            diag,
            std::forward<Handler>(handler)
        );
    }
};

struct initiate_start_execution
{
    template <class Handler, BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest>
    void operator()(
        Handler&& handler,
        channel& chan,
        const ExecutionRequest& req,
        execution_processor& st,
        diagnostics& diag
    )
    {
        serialize_execution_request(req, chan);
        async_start_execution_impl(
            chan,
            check_client_errors(req),
            get_encoding(req),
            st,
            diag,
            std::forward<Handler>(handler)
        );
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

template <BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest>
void boost::mysql::detail::execute(
    channel& channel,
    const ExecutionRequest& req,
    execution_processor& result,
    error_code& err,
    diagnostics& diag
)
{
    serialize_execution_request(req, channel);
    execute_impl(channel, check_client_errors(req), get_encoding(req), result, err, diag);
}

template <
    BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::detail::async_execute(
    channel& chan,
    ExecutionRequest&& req,
    execution_processor& result,
    diagnostics& diag,
    CompletionToken&& token
)
{
    return boost::asio::async_initiate<CompletionToken, void(boost::mysql::error_code)>(
        initiate_execute(),
        token,
        std::ref(chan),
        std::forward<ExecutionRequest>(req),
        std::ref(result),
        std::ref(diag)
    );
}

template <BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest>
void boost::mysql::detail::start_execution(
    channel& channel,
    const ExecutionRequest& req,
    execution_processor& st,
    error_code& err,
    diagnostics& diag
)
{
    serialize_execution_request(req, channel);
    start_execution_impl(channel, check_client_errors(req), get_encoding(req), st, err, diag);
}

template <
    BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::detail::async_start_execution(
    channel& chan,
    ExecutionRequest&& req,
    execution_processor& st,
    diagnostics& diag,
    CompletionToken&& token
)
{
    return boost::asio::async_initiate<CompletionToken, void(boost::mysql::error_code)>(
        initiate_start_execution(),
        token,
        std::ref(chan),
        std::forward<ExecutionRequest>(req),
        std::ref(st),
        std::ref(diag)
    );
}

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_QUERY_HPP_ */
