//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_HIGH_LEVEL_EXECUTION_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_HIGH_LEVEL_EXECUTION_HPP

#pragma once
#include <boost/mysql/detail/network_algorithms/execute.hpp>
#include <boost/mysql/detail/network_algorithms/high_level_execution.hpp>
#include <boost/mysql/detail/network_algorithms/start_execution.hpp>
#include <boost/mysql/detail/protocol/prepared_statement_messages.hpp>
#include <boost/mysql/detail/protocol/query_messages.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>

#include <boost/asio/bind_executor.hpp>

#include <functional>
#include <tuple>

namespace boost {
namespace mysql {
namespace detail {

// Helpers
inline void serialize_query_exec_req(channel_base& chan, string_view query)
{
    com_query_packet request{string_eof(query)};
    serialize_message(request, chan.current_capabilities(), chan.shared_buffer());
}

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

template <BOOST_MYSQL_FIELD_LIKE... T, std::size_t... I>
std::array<field_view, sizeof...(T)> tuple_to_array_impl(const std::tuple<T...>& t, boost::mp11::index_sequence<I...>) noexcept
{
    return std::array<field_view, sizeof...(T)>{{field_view(std::get<I>(t))...}};
}

template <BOOST_MYSQL_FIELD_LIKE... T>
std::array<field_view, sizeof...(T)> tuple_to_array(const std::tuple<T...>& t) noexcept
{
    return tuple_to_array_impl(t, boost::mp11::make_index_sequence<sizeof...(T)>());
}

template <BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple>
void serialize_stmt_exec_req(channel_base& chan, const statement& stmt, const FieldLikeTuple& params)
{
    auto arr = tuple_to_array(params);
    serialize_stmt_exec_req(chan, stmt, arr.begin(), arr.end());
}

inline error_code check_num_params(const statement& stmt, std::size_t param_count)
{
    return param_count == stmt.num_params() ? error_code() : make_error_code(client_errc::wrong_num_params);
}

template <BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator>
error_code check_num_params(
    const statement& stmt,
    FieldViewFwdIterator params_first,
    FieldViewFwdIterator params_last
)
{
    return check_num_params(stmt, std::distance(params_first, params_last));
}

template <BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple>
error_code check_num_params(const statement& stmt, const FieldLikeTuple&)
{
    return check_num_params(stmt, std::tuple_size<FieldLikeTuple>::value);
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
struct initiate_query
{
    template <class Handler, class Stream>
    void operator()(
        Handler&& handler,
        std::reference_wrapper<channel<Stream>> chan,
        string_view query,
        results& result,
        diagnostics& diag
    )
    {
        serialize_query_exec_req(chan, query);
        async_execute(chan.get(), resultset_encoding::text, result, diag, std::forward<Handler>(handler));
    }
};

struct initiate_start_query
{
    template <class Handler, class Stream>
    void operator()(
        Handler&& handler,
        std::reference_wrapper<channel<Stream>> chan,
        string_view query,
        execution_state& st,
        diagnostics& diag
    )
    {
        serialize_query_exec_req(chan, query);
        async_start_execution(chan.get(), resultset_encoding::text, st, diag, std::forward<Handler>(handler));
    }
};

struct initiate_execute_statement
{
    template <class Handler, class Stream, BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple>
    void operator()(
        Handler&& handler,
        std::reference_wrapper<channel<Stream>> chan,
        const statement& stmt,
        const FieldLikeTuple& params,
        results& result,
        diagnostics& diag
    )
    {
        auto ec = check_num_params(stmt, params);
        if (ec)
        {
            diag.clear();
            fast_fail(chan.get(), std::forward<Handler>(handler), ec);
        }
        else
        {
            serialize_stmt_exec_req(chan, stmt, params);
            async_execute(
                chan.get(),
                resultset_encoding::binary,
                result,
                diag,
                std::forward<Handler>(handler)
            );
        }
    }
};

struct initiate_start_statement_execution
{
    template <class Handler, class Stream, BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple>
    void operator()(
        Handler&& handler,
        std::reference_wrapper<channel<Stream>> chan,
        const statement& stmt,
        const FieldLikeTuple& params,
        execution_state& st,
        diagnostics& diag
    )
    {
        auto ec = check_num_params(stmt, params);
        if (ec)
        {
            diag.clear();
            fast_fail(chan.get(), std::forward<Handler>(handler), ec);
        }
        else
        {
            serialize_stmt_exec_req(chan, stmt, params);
            async_start_execution(
                chan.get(),
                resultset_encoding::binary,
                st,
                diag,
                std::forward<Handler>(handler)
            );
        }
    }

    template <class Handler, class Stream, BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FwdIt>
    void operator()(
        Handler&& handler,
        std::reference_wrapper<channel<Stream>> chan,
        const statement& stmt,
        FwdIt first,
        FwdIt last,
        execution_state& st,
        diagnostics& diag
    )
    {
        auto ec = check_num_params(stmt, first, last);
        if (ec)
        {
            diag.clear();
            fast_fail(chan.get(), std::forward<Handler>(handler), ec);
        }
        else
        {
            serialize_stmt_exec_req(chan, stmt, first, last);
            async_start_execution(
                chan.get(),
                resultset_encoding::binary,
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

template <class Stream>
void boost::mysql::detail::query(
    channel<Stream>& channel,
    string_view query,
    results& result,
    error_code& err,
    diagnostics& diag
)
{
    serialize_query_exec_req(channel, query);
    execute(channel, resultset_encoding::text, result, err, diag);
}

template <class Stream, BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::detail::async_query(
    channel<Stream>& chan,
    string_view query,
    results& result,
    diagnostics& diag,
    CompletionToken&& token
)
{
    return boost::asio::async_initiate<CompletionToken, void(boost::mysql::error_code)>(
        initiate_query(),
        token,
        std::ref(chan),
        query,
        std::ref(result),
        std::ref(diag)
    );
}

template <class Stream>
void boost::mysql::detail::start_query(
    channel<Stream>& channel,
    string_view query,
    execution_state& st,
    error_code& err,
    diagnostics& diag
)
{
    serialize_query_exec_req(channel, query);
    start_execution(channel, resultset_encoding::text, st, err, diag);
}

template <class Stream, BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::detail::async_start_query(
    channel<Stream>& chan,
    string_view query,
    execution_state& st,
    diagnostics& diag,
    CompletionToken&& token
)
{
    return boost::asio::async_initiate<CompletionToken, void(boost::mysql::error_code)>(
        initiate_start_query(),
        token,
        std::ref(chan),
        query,
        std::ref(st),
        std::ref(diag)
    );
}

template <class Stream, BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple>
void boost::mysql::detail::execute_statement(
    channel<Stream>& channel,
    const statement& stmt,
    const FieldLikeTuple& params,
    results& result,
    error_code& err,
    diagnostics& diag
)
{
    err = check_num_params(stmt, params);
    if (err)
        return;

    serialize_stmt_exec_req(channel, stmt, params);
    execute(channel, resultset_encoding::binary, result, err, diag);
}

template <
    class Stream,
    BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::detail::async_execute_statement(
    channel<Stream>& chan,
    const statement& stmt,
    FieldLikeTuple&& params,
    results& result,
    diagnostics& diag,
    CompletionToken&& token
)
{
    return boost::asio::async_initiate<CompletionToken, void(boost::mysql::error_code)>(
        initiate_execute_statement(),
        token,
        std::ref(chan),
        stmt,
        std::forward<FieldLikeTuple>(params),
        std::ref(result),
        std::ref(diag)
    );
}

template <class Stream, BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple>
void boost::mysql::detail::start_statement_execution(
    channel<Stream>& channel,
    const statement& stmt,
    const FieldLikeTuple& params,
    execution_state& st,
    error_code& err,
    diagnostics& diag
)
{
    err = check_num_params(stmt, params);
    if (err)
        return;

    serialize_stmt_exec_req(channel, stmt, params);
    start_execution(channel, resultset_encoding::binary, st, err, diag);
}

template <
    class Stream,
    BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::detail::async_start_statement_execution(
    channel<Stream>& chan,
    const statement& stmt,
    FieldLikeTuple&& params,
    execution_state& output,
    diagnostics& diag,
    CompletionToken&& token
)
{
    return boost::asio::async_initiate<CompletionToken, void(boost::mysql::error_code)>(
        initiate_start_statement_execution(),
        token,
        std::ref(chan),
        stmt,
        std::forward<FieldLikeTuple>(params),
        std::ref(output),
        std::ref(diag)
    );
}

template <class Stream, BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator>
void boost::mysql::detail::start_statement_execution(
    channel<Stream>& chan,
    const statement& stmt,
    FieldViewFwdIterator params_first,
    FieldViewFwdIterator params_last,
    execution_state& st,
    error_code& err,
    diagnostics& diag
)
{
    err = check_num_params(stmt, params_first, params_last);
    if (err)
        return;

    serialize_stmt_exec_req(chan, stmt, params_first, params_last);
    start_execution(chan, resultset_encoding::binary, st, err, diag);
}

template <
    class Stream,
    BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::detail::async_start_statement_execution(
    channel<Stream>& chan,
    const statement& stmt,
    FieldViewFwdIterator params_first,
    FieldViewFwdIterator params_last,
    execution_state& st,
    diagnostics& diag,
    CompletionToken&& token
)
{
    return boost::asio::async_initiate<CompletionToken, void(boost::mysql::error_code)>(
        initiate_start_statement_execution(),
        token,
        std::ref(chan),
        stmt,
        params_first,
        params_last,
        std::ref(st),
        std::ref(diag)
    );
}

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_QUERY_HPP_ */
