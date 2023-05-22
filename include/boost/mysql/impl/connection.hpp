//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_CONNECTION_HPP
#define BOOST_MYSQL_IMPL_CONNECTION_HPP

#pragma once

#include <boost/mysql/connection.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/row.hpp>
#include <boost/mysql/statement.hpp>

#include <boost/mysql/detail/auxiliar/access_fwd.hpp>
#include <boost/mysql/detail/auxiliar/error_helpers.hpp>
#include <boost/mysql/detail/network_algorithms/close_connection.hpp>
#include <boost/mysql/detail/network_algorithms/close_statement.hpp>
#include <boost/mysql/detail/network_algorithms/connect.hpp>
#include <boost/mysql/detail/network_algorithms/handshake.hpp>
#include <boost/mysql/detail/network_algorithms/high_level_execution.hpp>
#include <boost/mysql/detail/network_algorithms/ping.hpp>
#include <boost/mysql/detail/network_algorithms/prepare_statement.hpp>
#include <boost/mysql/detail/network_algorithms/quit_connection.hpp>
#include <boost/mysql/detail/network_algorithms/read_resultset_head.hpp>
#include <boost/mysql/detail/network_algorithms/read_some_rows_dynamic.hpp>
#include <boost/mysql/detail/network_algorithms/read_some_rows_impl.hpp>
#include <boost/mysql/detail/network_algorithms/read_some_rows_static.hpp>

#include <boost/asio/buffer.hpp>
#include <boost/assert/source_location.hpp>

#include <utility>

// connect
template <class Stream>
template <class EndpointType>
void boost::mysql::connection<Stream>::connect(
    const EndpointType& endpoint,
    const handshake_params& params,
    error_code& ec,
    diagnostics& diag
)
{
    detail::clear_errors(ec, diag);
    detail::connect(get_channel(), endpoint, params, ec, diag);
}

template <class Stream>
template <class EndpointType>
void boost::mysql::connection<Stream>::connect(const EndpointType& endpoint, const handshake_params& params)
{
    detail::error_block blk;
    detail::connect(get_channel(), endpoint, params, blk.err, blk.diag);
    blk.check(BOOST_CURRENT_LOCATION);
}

template <class Stream>
template <
    class EndpointType,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::connection<Stream>::async_connect(
    const EndpointType& endpoint,
    const handshake_params& params,
    diagnostics& diag,
    CompletionToken&& token
)
{
    return detail::async_connect(get_channel(), endpoint, params, diag, std::forward<CompletionToken>(token));
}

// handshake
template <class Stream>
void boost::mysql::connection<Stream>::handshake(
    const handshake_params& params,
    error_code& code,
    diagnostics& diag
)
{
    detail::clear_errors(code, diag);
    detail::handshake(get_channel(), params, code, diag);
}

template <class Stream>
void boost::mysql::connection<Stream>::handshake(const handshake_params& params)
{
    detail::error_block blk;
    handshake(params, blk.err, blk.diag);
    blk.check(BOOST_CURRENT_LOCATION);
}

template <class Stream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::connection<Stream>::async_handshake(
    const handshake_params& params,
    diagnostics& diag,
    CompletionToken&& token
)
{
    return detail::async_handshake(get_channel(), params, diag, std::forward<CompletionToken>(token));
}

template <class Stream>
void boost::mysql::connection<Stream>::start_query(
    string_view query_string,
    execution_state& result,
    error_code& err,
    diagnostics& diag
)
{
    detail::clear_errors(err, diag);
    detail::start_execution(get_channel(), query_string, detail::impl_access::get_impl(result), err, diag);
}

template <class Stream>
void boost::mysql::connection<Stream>::start_query(string_view query_string, execution_state& result)
{
    detail::error_block blk;
    detail::start_execution(
        get_channel(),
        query_string,
        detail::impl_access::get_impl(result),
        blk.err,
        blk.diag
    );
    blk.check(BOOST_CURRENT_LOCATION);
}

template <class Stream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::connection<Stream>::async_start_query(
    string_view query_string,
    execution_state& result,
    diagnostics& diag,
    CompletionToken&& token
)
{
    return detail::async_start_execution(
        get_channel(),
        query_string,
        detail::impl_access::get_impl(result).get_interface(),
        diag,
        std::forward<CompletionToken>(token)
    );
}

template <class Stream>
void boost::mysql::connection<Stream>::query(
    string_view query_string,
    results& result,
    error_code& err,
    diagnostics& diag
)
{
    detail::clear_errors(err, diag);
    detail::execute(
        get_channel(),
        query_string,
        detail::impl_access::get_impl(result).get_interface(),
        err,
        diag
    );
}

template <class Stream>
void boost::mysql::connection<Stream>::query(string_view query_string, results& result)
{
    detail::error_block blk;
    detail::execute(
        get_channel(),
        query_string,
        detail::impl_access::get_impl(result).get_interface(),
        blk.err,
        blk.diag
    );
    blk.check(BOOST_CURRENT_LOCATION);
}

template <class Stream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::connection<Stream>::async_query(
    string_view query_string,
    results& result,
    diagnostics& diag,
    CompletionToken&& token
)
{
    return detail::async_execute(
        get_channel(),
        query_string,
        detail::impl_access::get_impl(result).get_interface(),
        diag,
        std::forward<CompletionToken>(token)
    );
}

template <class Stream>
template <BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest, BOOST_MYSQL_RESULTS_TYPE ResultsType>
void boost::mysql::connection<Stream>::execute(
    const ExecutionRequest& req,
    ResultsType& result,
    error_code& err,
    diagnostics& diag
)
{
    detail::clear_errors(err, diag);
    detail::execute(get_channel(), req, detail::impl_access::get_impl(result).get_interface(), err, diag);
}

template <class Stream>
template <BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest, BOOST_MYSQL_RESULTS_TYPE ResultsType>
void boost::mysql::connection<Stream>::execute(const ExecutionRequest& req, ResultsType& result)
{
    detail::error_block blk;
    detail::execute(
        get_channel(),
        req,
        detail::impl_access::get_impl(result).get_interface(),
        blk.err,
        blk.diag
    );
    blk.check(BOOST_CURRENT_LOCATION);
}

template <class Stream>
template <
    BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest,
    BOOST_MYSQL_RESULTS_TYPE ResultsType,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::connection<Stream>::async_execute(
    ExecutionRequest&& req,
    ResultsType& result,
    diagnostics& diag,
    CompletionToken&& token
)
{
    return detail::async_execute(
        get_channel(),
        std::forward<ExecutionRequest>(req),
        detail::impl_access::get_impl(result).get_interface(),
        diag,
        std::forward<CompletionToken>(token)
    );
}

template <class Stream>
template <BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest, BOOST_MYSQL_EXECUTION_STATE_TYPE ExecutionStateType>
void boost::mysql::connection<Stream>::start_execution(
    const ExecutionRequest& req,
    ExecutionStateType& st,
    error_code& err,
    diagnostics& diag
)
{
    detail::clear_errors(err, diag);
    detail::start_execution(get_channel(), req, detail::impl_access::get_impl(st).get_interface(), err, diag);
}

template <class Stream>
template <BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest, BOOST_MYSQL_EXECUTION_STATE_TYPE ExecutionStateType>
void boost::mysql::connection<Stream>::start_execution(const ExecutionRequest& req, ExecutionStateType& st)
{
    detail::error_block blk;
    detail::start_execution(
        get_channel(),
        req,
        detail::impl_access::get_impl(st).get_interface(),
        blk.err,
        blk.diag
    );
    blk.check(BOOST_CURRENT_LOCATION);
}

template <class Stream>
template <
    BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest,
    BOOST_MYSQL_EXECUTION_STATE_TYPE ExecutionStateType,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::connection<Stream>::async_start_execution(
    ExecutionRequest&& req,
    ExecutionStateType& st,
    diagnostics& diag,
    CompletionToken&& token
)
{
    return detail::async_start_execution(
        get_channel(),
        std::forward<ExecutionRequest>(req),
        detail::impl_access::get_impl(st).get_interface(),
        diag,
        std::forward<CompletionToken>(token)
    );
}

// Prepare statement
template <class Stream>
boost::mysql::statement boost::mysql::connection<Stream>::prepare_statement(
    string_view stmt,
    error_code& err,
    diagnostics& diag
)
{
    detail::clear_errors(err, diag);
    return detail::prepare_statement(get_channel(), stmt, err, diag);
}

template <class Stream>
boost::mysql::statement boost::mysql::connection<Stream>::prepare_statement(string_view stmt)
{
    detail::error_block blk;
    auto res = detail::prepare_statement(get_channel(), stmt, blk.err, blk.diag);
    blk.check(BOOST_CURRENT_LOCATION);
    return res;
}

template <class Stream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::statement))
              CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code, boost::mysql::statement))
boost::mysql::connection<Stream>::async_prepare_statement(
    string_view stmt,
    diagnostics& diag,
    CompletionToken&& token
)
{
    return detail::async_prepare_statement(get_channel(), stmt, diag, std::forward<CompletionToken>(token));
}

// execute statement
template <class Stream>
template <BOOST_MYSQL_WRITABLE_FIELD_TUPLE WritableFieldTuple, class>
void boost::mysql::connection<Stream>::execute_statement(
    const statement& stmt,
    const WritableFieldTuple& params,
    results& result,
    error_code& err,
    diagnostics& diag
)
{
    detail::clear_errors(err, diag);
    detail::execute(
        get_channel(),
        stmt.bind(params),
        detail::impl_access::get_impl(result).get_interface(),
        err,
        diag
    );
}

template <class Stream>
template <BOOST_MYSQL_WRITABLE_FIELD_TUPLE WritableFieldTuple, class>
void boost::mysql::connection<Stream>::execute_statement(
    const statement& stmt,
    const WritableFieldTuple& params,
    results& result
)
{
    detail::error_block blk;
    detail::execute(
        get_channel(),
        stmt.bind(params),
        detail::impl_access::get_impl(result).get_interface(),
        blk.err,
        blk.diag
    );
    blk.check(BOOST_CURRENT_LOCATION);
}

template <class Stream>
template <
    BOOST_MYSQL_WRITABLE_FIELD_TUPLE WritableFieldTuple,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken,
    class>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::connection<Stream>::async_execute_statement(
    const statement& stmt,
    WritableFieldTuple&& params,
    results& result,
    diagnostics& diag,
    CompletionToken&& token
)
{
    return detail::async_execute(
        get_channel(),
        stmt.bind(std::forward<WritableFieldTuple>(params)),
        detail::impl_access::get_impl(result).get_interface(),
        diag,
        std::forward<CompletionToken>(token)
    );
}

template <class Stream>
template <BOOST_MYSQL_WRITABLE_FIELD_TUPLE WritableFieldTuple, class>
void boost::mysql::connection<Stream>::start_statement_execution(
    const statement& stmt,
    const WritableFieldTuple& params,
    execution_state& result,
    error_code& err,
    diagnostics& diag
)
{
    detail::clear_errors(err, diag);
    detail::start_execution(
        get_channel(),
        stmt.bind(params),
        detail::impl_access::get_impl(result).get_interface(),
        err,
        diag
    );
}

template <class Stream>
template <BOOST_MYSQL_WRITABLE_FIELD_TUPLE WritableFieldTuple, class>
void boost::mysql::connection<Stream>::start_statement_execution(
    const statement& stmt,
    const WritableFieldTuple& params,
    execution_state& result
)
{
    detail::error_block blk;
    detail::start_execution(
        get_channel(),
        stmt.bind(params),
        detail::impl_access::get_impl(result).get_interface(),
        blk.err,
        blk.diag
    );
    blk.check(BOOST_CURRENT_LOCATION);
}

template <class Stream>
template <
    BOOST_MYSQL_WRITABLE_FIELD_TUPLE WritableFieldTuple,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken,
    class>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::connection<Stream>::async_start_statement_execution(
    const statement& stmt,
    WritableFieldTuple&& params,
    execution_state& result,
    diagnostics& diag,
    CompletionToken&& token
)
{
    return detail::async_start_execution(
        get_channel(),
        stmt.bind(std::forward<WritableFieldTuple>(params)),
        detail::impl_access::get_impl(result).get_interface(),
        diag,
        std::forward<CompletionToken>(token)
    );
}

// Execute statement, with iterators
template <class Stream>
template <BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator>
void boost::mysql::connection<Stream>::start_statement_execution(
    const statement& stmt,
    FieldViewFwdIterator params_first,
    FieldViewFwdIterator params_last,
    execution_state& st,
    error_code& err,
    diagnostics& diag
)
{
    detail::clear_errors(err, diag);
    detail::start_execution(
        get_channel(),
        stmt.bind(params_first, params_last),
        detail::impl_access::get_impl(st).get_interface(),
        err,
        diag
    );
}

template <class Stream>
template <BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator>
void boost::mysql::connection<Stream>::start_statement_execution(
    const statement& stmt,
    FieldViewFwdIterator params_first,
    FieldViewFwdIterator params_last,
    execution_state& result
)
{
    detail::error_block blk;
    detail::start_execution(
        get_channel(),
        stmt.bind(params_first, params_last),
        detail::impl_access::get_impl(result).get_interface(),
        blk.err,
        blk.diag
    );
    blk.check(BOOST_CURRENT_LOCATION);
}

template <class Stream>
template <
    BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::connection<Stream>::async_start_statement_execution(
    const statement& stmt,
    FieldViewFwdIterator params_first,
    FieldViewFwdIterator params_last,
    execution_state& result,
    diagnostics& diag,
    CompletionToken&& token
)
{
    return detail::async_start_execution(
        get_channel(),
        stmt.bind(params_first, params_last),
        detail::impl_access::get_impl(result).get_interface(),
        diag,
        std::forward<CompletionToken>(token)
    );
}

// Close statement
template <class Stream>
void boost::mysql::connection<Stream>::close_statement(
    const statement& stmt,
    error_code& code,
    diagnostics& diag
)
{
    detail::clear_errors(code, diag);
    detail::close_statement(get_channel(), stmt, code, diag);
}

template <class Stream>
void boost::mysql::connection<Stream>::close_statement(const statement& stmt)
{
    detail::error_block blk;
    detail::close_statement(get_channel(), stmt, blk.err, blk.diag);
    blk.check(BOOST_CURRENT_LOCATION);
}

template <class Stream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::connection<Stream>::async_close_statement(
    const statement& stmt,
    diagnostics& diag,
    CompletionToken&& token
)
{
    return detail::async_close_statement(get_channel(), stmt, diag, std::forward<CompletionToken>(token));
}

// read resultset head
template <class Stream>
template <BOOST_MYSQL_EXECUTION_STATE_TYPE ExecutionStateType>
void boost::mysql::connection<Stream>::read_resultset_head(
    ExecutionStateType& st,
    error_code& err,
    diagnostics& diag
)
{
    detail::clear_errors(err, diag);
    return detail::read_resultset_head(
        get_channel(),
        detail::impl_access::get_impl(st).get_interface(),
        err,
        diag
    );
}

template <class Stream>
template <BOOST_MYSQL_EXECUTION_STATE_TYPE ExecutionStateType>
void boost::mysql::connection<Stream>::read_resultset_head(ExecutionStateType& st)
{
    detail::error_block blk;
    detail::read_resultset_head(
        get_channel(),
        detail::impl_access::get_impl(st).get_interface(),
        blk.err,
        blk.diag
    );
    blk.check(BOOST_CURRENT_LOCATION);
}

template <class Stream>
template <
    BOOST_MYSQL_EXECUTION_STATE_TYPE ExecutionStateType,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::connection<Stream>::async_read_resultset_head(
    ExecutionStateType& st,
    diagnostics& diag,
    CompletionToken&& token
)
{
    return detail::async_read_resultset_head(
        get_channel(),
        detail::impl_access::get_impl(st).get_interface(),
        diag,
        std::forward<CompletionToken>(token)
    );
}

// read some rows
template <class Stream>
boost::mysql::rows_view boost::mysql::connection<Stream>::read_some_rows(
    execution_state& st,
    error_code& err,
    diagnostics& diag
)
{
    detail::clear_errors(err, diag);
    return detail::read_some_rows_dynamic(
        get_channel(),
        detail::impl_access::get_impl(st).get_interface(),
        err,
        diag
    );
}

template <class Stream>
boost::mysql::rows_view boost::mysql::connection<Stream>::read_some_rows(execution_state& st)
{
    detail::error_block blk;
    rows_view res = detail::read_some_rows_dynamic(
        get_channel(),
        detail::impl_access::get_impl(st).get_interface(),
        blk.err,
        blk.diag
    );
    blk.check(BOOST_CURRENT_LOCATION);
    return res;
}

template <class Stream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::rows_view))
              CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code, boost::mysql::rows_view))
boost::mysql::connection<Stream>::async_read_some_rows(
    execution_state& st,
    diagnostics& diag,
    CompletionToken&& token
)
{
    return detail::async_read_some_rows_dynamic(
        get_channel(),
        detail::impl_access::get_impl(st).get_interface(),
        diag,
        std::forward<CompletionToken>(token)
    );
}

#ifdef BOOST_MYSQL_CXX14
template <class Stream>
template <class SpanRowType, class... RowType>
std::size_t boost::mysql::connection<Stream>::read_some_rows(
    static_execution_state<RowType...>& st,
    span<SpanRowType> output,
    error_code& err,
    diagnostics& diag
)
{
    detail::clear_errors(err, diag);
    return detail::read_some_rows_static(get_channel(), st, output, err, diag);
}

template <class Stream>
template <class SpanRowType, class... RowType>
std::size_t boost::mysql::connection<Stream>::read_some_rows(
    static_execution_state<RowType...>& st,
    span<SpanRowType> output
)
{
    detail::error_block blk;
    auto res = detail::read_some_rows_static(get_channel(), st, output, blk.err, blk.diag);
    blk.check(BOOST_CURRENT_LOCATION);
    return res;
}

template <class Stream>
template <
    class SpanRowType,
    class... RowType,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, std::size_t)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code, std::size_t))
boost::mysql::connection<Stream>::async_read_some_rows(
    static_execution_state<RowType...>& st,
    span<SpanRowType> output,
    diagnostics& diag,
    CompletionToken&& token
)
{
    return detail::async_read_some_rows_static(
        get_channel(),
        st,
        output,
        diag,
        std::forward<CompletionToken>(token)
    );
}
#endif

// ping
template <class Stream>
void boost::mysql::connection<Stream>::ping(error_code& err, diagnostics& diag)
{
    detail::clear_errors(err, diag);
    detail::ping(get_channel(), err, diag);
}

template <class Stream>
void boost::mysql::connection<Stream>::ping()
{
    detail::error_block blk;
    detail::ping(get_channel(), blk.err, blk.diag);
    blk.check(BOOST_CURRENT_LOCATION);
}

template <class Stream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::connection<Stream>::async_ping(diagnostics& diag, CompletionToken&& token)
{
    return detail::async_ping(get_channel(), diag, std::forward<CompletionToken>(token));
}

// Close
template <class Stream>
void boost::mysql::connection<Stream>::close(error_code& err, diagnostics& diag)
{
    detail::clear_errors(err, diag);
    detail::close_connection(get_channel(), err, diag);
}

template <class Stream>
void boost::mysql::connection<Stream>::close()
{
    detail::error_block blk;
    detail::close_connection(get_channel(), blk.err, blk.diag);
    blk.check(BOOST_CURRENT_LOCATION);
}

template <class Stream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::connection<Stream>::async_close(diagnostics& diag, CompletionToken&& token)
{
    return detail::async_close_connection(get_channel(), diag, std::forward<CompletionToken>(token));
}

// quit
template <class Stream>
void boost::mysql::connection<Stream>::quit(error_code& err, diagnostics& diag)
{
    detail::clear_errors(err, diag);
    detail::quit_connection(get_channel(), err, diag);
}

template <class Stream>
void boost::mysql::connection<Stream>::quit()
{
    detail::error_block blk;
    detail::quit_connection(get_channel(), blk.err, blk.diag);
    blk.check(BOOST_CURRENT_LOCATION);
}

template <class Stream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::connection<Stream>::async_quit(diagnostics& diag, CompletionToken&& token)
{
    return detail::async_quit_connection(get_channel(), diag, std::forward<CompletionToken>(token));
}

struct boost::mysql::detail::connection_access
{
    // Exposed for testing
    template <class Stream>
    static channel<Stream>& get_channel(connection<Stream>& conn) noexcept
    {
        return conn.get_channel();
    }
};

#endif
