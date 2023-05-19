//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_HIGH_LEVEL_EXECUTION_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_HIGH_LEVEL_EXECUTION_HPP

#pragma once

#include <boost/mysql/detail/auxiliar/execution_request.hpp>
#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/network_algorithms/execute_impl.hpp>
#include <boost/mysql/detail/network_algorithms/high_level_execution.hpp>
#include <boost/mysql/detail/network_algorithms/start_execution_impl.hpp>
#include <boost/mysql/detail/protocol/prepared_statement_messages.hpp>
#include <boost/mysql/detail/protocol/query_messages.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>
#include <boost/mysql/detail/typing/writable_field_traits.hpp>

#include <boost/asio/bind_executor.hpp>

#include <functional>
#include <tuple>

namespace boost {
namespace mysql {
namespace detail {

// Text queries
template <
    typename T,
    typename EnableIf = typename std::enable_if<std::is_convertible<T, string_view>::value>::type>
resultset_encoding get_encoding(const T&)
{
    return resultset_encoding::text;
}

template <
    typename T,
    typename EnableIf = typename std::enable_if<std::is_convertible<T, string_view>::value>::type>
void serialize_execution_request(const T& req, channel_base& chan)
{
    com_query_packet request{string_eof(string_view(req))};
    serialize_message(request, chan.current_capabilities(), chan.shared_buffer());
}

template <
    typename T,
    typename EnableIf = typename std::enable_if<std::is_convertible<T, string_view>::value>::type>
error_code check_client_errors(const T&)
{
    return error_code();
}

// Helpers for statements
template <BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator>
void serialize_stmt_exec_req(
    channel_base& chan,
    const statement& stmt,
    FieldViewFwdIterator first,
    FieldViewFwdIterator last
)
{
    com_stmt_execute_packet<FieldViewFwdIterator> request{
        stmt.id(),
        std::uint8_t(0),   // flags
        std::uint32_t(1),  // iteration count
        std::uint8_t(1),   // new params flag: set
        first,
        last,
    };
    serialize_message(request, chan.current_capabilities(), chan.shared_buffer());
}

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
template <BOOST_MYSQL_WRITABLE_FIELD_TUPLE WritableFieldTuple>
resultset_encoding get_encoding(const bound_statement_tuple<WritableFieldTuple>&)
{
    return resultset_encoding::binary;
}

template <BOOST_MYSQL_WRITABLE_FIELD_TUPLE WritableFieldTuple>
void serialize_execution_request(const bound_statement_tuple<WritableFieldTuple>& req, channel_base& chan)
{
    const auto& impl = statement_access::get_impl_tuple(req);
    auto arr = tuple_to_array(impl.params);
    serialize_stmt_exec_req(chan, impl.stmt, arr.begin(), arr.end());
}

template <BOOST_MYSQL_WRITABLE_FIELD_TUPLE WritableFieldTuple>
error_code check_client_errors(const bound_statement_tuple<WritableFieldTuple>& req)
{
    return check_num_params(
        statement_access::get_impl_tuple(req).stmt,
        std::tuple_size<WritableFieldTuple>::value
    );
}

// Statement, iterators
template <BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator>
resultset_encoding get_encoding(const bound_statement_iterator_range<FieldViewFwdIterator>&)
{
    return resultset_encoding::binary;
}

template <BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator>
void serialize_execution_request(
    const bound_statement_iterator_range<FieldViewFwdIterator>& req,
    channel_base& chan
)
{
    const auto& impl = statement_access::get_impl_range(req);
    serialize_stmt_exec_req(chan, impl.stmt, impl.first, impl.last);
}

template <BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator>
error_code check_client_errors(const bound_statement_iterator_range<FieldViewFwdIterator>& req)
{
    const auto& impl = statement_access::get_impl_range(req);
    return check_num_params(impl.stmt, std::distance(impl.first, impl.last));
}

template <class Stream, class Handler>
void fast_fail(channel<Stream>& chan, Handler&& handler, error_code ec)
{
    boost::asio::post(boost::asio::bind_executor(
        boost::asio::get_associated_executor(handler, chan.get_executor()),
        std::bind(std::forward<Handler>(handler), ec)
    ));
}

// Async initiations
struct initiate_execute
{
    template <class Handler, BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest, class Stream>
    void operator()(
        Handler&& handler,
        std::reference_wrapper<channel<Stream>> chan,
        const ExecutionRequest& req,
        execution_processor& result,
        diagnostics& diag
    )
    {
        auto ec = check_client_errors(req);
        if (ec)
        {
            diag.clear();
            fast_fail(chan.get(), std::forward<Handler>(handler), ec);
        }
        else
        {
            serialize_execution_request(req, chan);
            async_execute_impl(chan.get(), get_encoding(req), result, diag, std::forward<Handler>(handler));
        }
    }
};

struct initiate_start_execution
{
    template <class Handler, BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest, class Stream>
    void operator()(
        Handler&& handler,
        std::reference_wrapper<channel<Stream>> chan,
        const ExecutionRequest& req,
        execution_processor& st,
        diagnostics& diag
    )
    {
        auto ec = check_client_errors(req);
        if (ec)
        {
            diag.clear();
            fast_fail(chan.get(), std::forward<Handler>(handler), ec);
        }
        else
        {
            serialize_execution_request(req, chan);
            async_start_execution_impl(
                chan.get(),
                get_encoding(req),
                st,
                diag,
                std::forward<Handler>(handler)
            );
        }
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

template <class Stream, BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest>
void boost::mysql::detail::execute(
    channel<Stream>& channel,
    const ExecutionRequest& req,
    execution_processor& result,
    error_code& err,
    diagnostics& diag
)
{
    diag.clear();
    err = check_client_errors(req);
    if (err)
        return;

    serialize_execution_request(req, channel);
    execute_impl(channel, get_encoding(req), result, err, diag);
}

template <
    class Stream,
    BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::detail::async_execute(
    channel<Stream>& chan,
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

template <class Stream, BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest>
void boost::mysql::detail::start_execution(
    channel<Stream>& channel,
    const ExecutionRequest& req,
    execution_processor& st,
    error_code& err,
    diagnostics& diag
)
{
    diag.clear();
    err = check_client_errors(req);
    if (err)
        return;

    serialize_execution_request(req, channel);
    start_execution_impl(channel, get_encoding(req), st, err, diag);
}

template <
    class Stream,
    BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::detail::async_start_execution(
    channel<Stream>& chan,
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
