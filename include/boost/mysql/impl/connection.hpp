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

#include <boost/mysql/detail/auxiliar/error_helpers.hpp>
#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/execution_processor/execution_state_impl.hpp>
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
namespace boost {
namespace mysql {
namespace detail {

// Handles casting from the generic EndpointType we've got in the interface to the concrete endpoint type
template <class Stream>
void connect(
    channel& chan,
    const typename Stream::lowest_layer_type::endpoint_type& ep,
    const handshake_params& params,
    error_code& err,
    diagnostics& diag
)
{
    connect_impl(chan, &ep, params, err, diag);
}

template <class Stream>
struct connect_initiation
{
    template <class Handler>
    void operator()(
        Handler&& handler,
        channel* chan,
        const typename Stream::lowest_layer_type::endpoint_type& endpoint,
        handshake_params params,
        diagnostics* diag
    )
    {
        async_connect_impl(*chan, &endpoint, params, *diag, std::forward<Handler>(handler));
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

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
    detail::connect<Stream>(channel_.get(), endpoint, params, ec, diag);
}

template <class Stream>
template <class EndpointType>
void boost::mysql::connection<Stream>::connect(const EndpointType& endpoint, const handshake_params& params)
{
    detail::error_block blk;
    detail::connect<Stream>(channel_.get(), endpoint, params, blk.err, blk.diag);
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
    return boost::asio::async_initiate<CompletionToken, void(error_code)>(
        detail::connect_initiation<Stream>(),
        token,
        &channel_.get(),
        endpoint,
        params,
        &diag
    );
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
    detail::handshake_impl(channel_.get(), params, code, diag);
}

template <class Stream>
void boost::mysql::connection<Stream>::handshake(const handshake_params& params)
{
    detail::error_block blk;
    detail::handshake_impl(channel_.get(), params, blk.err, blk.diag);
    blk.check(BOOST_CURRENT_LOCATION);
}

namespace boost {
namespace mysql {
namespace detail {

struct handshake_initiation
{
    template <class Handler>
    void operator()(Handler&& handler, channel* chan, handshake_params params, diagnostics* diag)
    {
        async_handshake_impl(*chan, params, *diag, std::forward<Handler>(handler));
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

template <class Stream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::connection<Stream>::async_handshake(
    const handshake_params& params,
    diagnostics& diag,
    CompletionToken&& token
)
{
    return boost::asio::async_initiate<CompletionToken, void(error_code)>(
        detail::handshake_initiation(),
        token,
        &channel_.get(),
        params,
        &diag
    );
}

// start_query
template <class Stream>
void boost::mysql::connection<Stream>::start_query(
    string_view query_string,
    execution_state& result,
    error_code& err,
    diagnostics& diag
)
{
    detail::clear_errors(err, diag);
    detail::start_execution(channel_.get(), query_string, detail::impl_access::get_impl(result), err, diag);
}

template <class Stream>
void boost::mysql::connection<Stream>::start_query(string_view query_string, execution_state& result)
{
    detail::error_block blk;
    detail::start_execution(
        channel_.get(),
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
        channel_.get(),
        query_string,
        detail::impl_access::get_impl(result).get_interface(),
        diag,
        std::forward<CompletionToken>(token)
    );
}

// query
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
        channel_.get(),
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
        channel_.get(),
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
        channel_.get(),
        query_string,
        detail::impl_access::get_impl(result).get_interface(),
        diag,
        std::forward<CompletionToken>(token)
    );
}

// execute
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
    detail::execute(channel_.get(), req, detail::impl_access::get_impl(result).get_interface(), err, diag);
}

template <class Stream>
template <BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest, BOOST_MYSQL_RESULTS_TYPE ResultsType>
void boost::mysql::connection<Stream>::execute(const ExecutionRequest& req, ResultsType& result)
{
    detail::error_block blk;
    detail::execute(
        channel_.get(),
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
        channel_.get(),
        std::forward<ExecutionRequest>(req),
        detail::impl_access::get_impl(result).get_interface(),
        diag,
        std::forward<CompletionToken>(token)
    );
}

// start_execution
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
    detail::start_execution(
        channel_.get(),
        req,
        detail::impl_access::get_impl(st).get_interface(),
        err,
        diag
    );
}

template <class Stream>
template <BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest, BOOST_MYSQL_EXECUTION_STATE_TYPE ExecutionStateType>
void boost::mysql::connection<Stream>::start_execution(const ExecutionRequest& req, ExecutionStateType& st)
{
    detail::error_block blk;
    detail::start_execution(
        channel_.get(),
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
        channel_.get(),
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
    return detail::prepare_statement_impl(channel_.get(), stmt, err, diag);
}

template <class Stream>
boost::mysql::statement boost::mysql::connection<Stream>::prepare_statement(string_view stmt)
{
    detail::error_block blk;
    auto res = detail::prepare_statement_impl(channel_.get(), stmt, blk.err, blk.diag);
    blk.check(BOOST_CURRENT_LOCATION);
    return res;
}

namespace boost {
namespace mysql {
namespace detail {

struct prepare_statement_initiation
{
    template <class Handler>
    void operator()(Handler&& handler, channel* chan, string_view stmt_sql, diagnostics* diag)
    {
        async_prepare_statement_impl(*chan, stmt_sql, *diag, std::forward<Handler>(handler));
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

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
    return boost::asio::async_initiate<CompletionToken, void(error_code, statement)>(
        detail::prepare_statement_initiation(),
        token,
        &channel_.get(),
        stmt,
        &diag
    );
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
        channel_.get(),
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
        channel_.get(),
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
        channel_.get(),
        stmt.bind(std::forward<WritableFieldTuple>(params)),
        detail::impl_access::get_impl(result).get_interface(),
        diag,
        std::forward<CompletionToken>(token)
    );
}

// start_statement_execution
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
        channel_.get(),
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
        channel_.get(),
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
        channel_.get(),
        stmt.bind(std::forward<WritableFieldTuple>(params)),
        detail::impl_access::get_impl(result).get_interface(),
        diag,
        std::forward<CompletionToken>(token)
    );
}

// start_statement_execution, with iterators
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
        channel_.get(),
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
        channel_.get(),
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
        channel_.get(),
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
    detail::close_statement_impl(channel_.get(), stmt, code, diag);
}

template <class Stream>
void boost::mysql::connection<Stream>::close_statement(const statement& stmt)
{
    detail::error_block blk;
    detail::close_statement_impl(channel_.get(), stmt, blk.err, blk.diag);
    blk.check(BOOST_CURRENT_LOCATION);
}

namespace boost {
namespace mysql {
namespace detail {

struct close_statement_initiation
{
    template <class Handler>
    void operator()(Handler&& handler, channel* chan, statement stmt, diagnostics* diag)
    {
        async_close_statement_impl(*chan, stmt, *diag, std::forward<Handler>(handler));
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

template <class Stream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::connection<Stream>::async_close_statement(
    const statement& stmt,
    diagnostics& diag,
    CompletionToken&& token
)
{
    return boost::asio::async_initiate<CompletionToken, void(error_code)>(
        detail::close_statement_initiation(),
        token,
        &channel_.get(),
        stmt,
        &diag
    );
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
    return detail::read_resultset_head_impl(
        channel_.get(),
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
    detail::read_resultset_head_impl(
        channel_.get(),
        detail::impl_access::get_impl(st).get_interface(),
        blk.err,
        blk.diag
    );
    blk.check(BOOST_CURRENT_LOCATION);
}

namespace boost {
namespace mysql {
namespace detail {

struct read_resultset_head_initiation
{
    template <class Handler>
    void operator()(Handler&& handler, channel* chan, execution_processor* proc, diagnostics* diag)
    {
        async_read_resultset_head_impl(*chan, *proc, *diag, std::forward<Handler>(handler));
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

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
    return boost::asio::async_initiate<CompletionToken, void(error_code)>(
        detail::read_resultset_head_initiation(),
        token,
        &channel_.get(),
        detail::impl_access::get_impl(st).get_interface(),
        &diag
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
    return detail::read_some_rows_dynamic_impl(
        channel_.get(),
        detail::impl_access::get_impl(st).get_interface(),
        err,
        diag
    );
}

template <class Stream>
boost::mysql::rows_view boost::mysql::connection<Stream>::read_some_rows(execution_state& st)
{
    detail::error_block blk;
    rows_view res = detail::read_some_rows_dynamic_impl(
        channel_.get(),
        detail::impl_access::get_impl(st).get_interface(),
        blk.err,
        blk.diag
    );
    blk.check(BOOST_CURRENT_LOCATION);
    return res;
}

namespace boost {
namespace mysql {
namespace detail {

struct read_some_rows_dynamic_initiation
{
    template <class Handler>
    void operator()(Handler&& handler, channel* chan, execution_state_impl* st, diagnostics* diag)
    {
        async_read_some_rows_dynamic_impl(*chan, *st, *diag, std::forward<Handler>(handler));
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

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
    return boost::asio::async_initiate<CompletionToken, void(error_code, rows_view)>(
        detail::read_some_rows_dynamic_initiation(),
        token,
        &channel_.get(),
        detail::impl_access::get_impl(st).get_interface(),
        &diag
    );
}

// read_some_rows, static interface
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
    return detail::read_some_rows_static(channel_.get(), st, output, err, diag);
}

template <class Stream>
template <class SpanRowType, class... RowType>
std::size_t boost::mysql::connection<Stream>::read_some_rows(
    static_execution_state<RowType...>& st,
    span<SpanRowType> output
)
{
    detail::error_block blk;
    auto res = detail::read_some_rows_static(channel_.get(), st, output, blk.err, blk.diag);
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
        channel_.get(),
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
    detail::ping_impl(channel_.get(), err, diag);
}

template <class Stream>
void boost::mysql::connection<Stream>::ping()
{
    detail::error_block blk;
    detail::ping_impl(channel_.get(), blk.err, blk.diag);
    blk.check(BOOST_CURRENT_LOCATION);
}

namespace boost {
namespace mysql {
namespace detail {

struct ping_initiation
{
    template <class Handler>
    void operator()(Handler&& handler, channel* chan, diagnostics* diag)
    {
        async_ping_impl(*chan, *diag, std::forward<Handler>(handler));
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

template <class Stream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::connection<Stream>::async_ping(diagnostics& diag, CompletionToken&& token)
{
    return boost::asio::async_initiate<CompletionToken, void(error_code)>(
        detail::ping_initiation(),
        token,
        &channel_.get(),
        &diag
    );
}

// Close connection
template <class Stream>
void boost::mysql::connection<Stream>::close(error_code& err, diagnostics& diag)
{
    detail::clear_errors(err, diag);
    detail::close_connection_impl(channel_.get(), err, diag);
}

template <class Stream>
void boost::mysql::connection<Stream>::close()
{
    detail::error_block blk;
    detail::close_connection_impl(channel_.get(), blk.err, blk.diag);
    blk.check(BOOST_CURRENT_LOCATION);
}

namespace boost {
namespace mysql {
namespace detail {

struct close_connection_initiation
{
    template <class Handler>
    void operator()(Handler&& handler, channel* chan, diagnostics* diag)
    {
        async_close_connection_impl(*chan, *diag, std::forward<Handler>(handler));
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

template <class Stream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::connection<Stream>::async_close(diagnostics& diag, CompletionToken&& token)
{
    return boost::asio::async_initiate<CompletionToken, void(error_code)>(
        detail::close_connection_initiation(),
        token,
        &channel_.get(),
        &diag
    );
}

// quit
template <class Stream>
void boost::mysql::connection<Stream>::quit(error_code& err, diagnostics& diag)
{
    detail::clear_errors(err, diag);
    detail::quit_connection_impl(channel_.get(), err, diag);
}

template <class Stream>
void boost::mysql::connection<Stream>::quit()
{
    detail::error_block blk;
    detail::quit_connection_impl(channel_.get(), blk.err, blk.diag);
    blk.check(BOOST_CURRENT_LOCATION);
}

namespace boost {
namespace mysql {
namespace detail {

struct quit_connection_initiation
{
    template <class Handler>
    void operator()(Handler&& handler, channel* chan, diagnostics* diag)
    {
        async_quit_connection_impl(*chan, *diag, std::forward<Handler>(handler));
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

template <class Stream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::connection<Stream>::async_quit(diagnostics& diag, CompletionToken&& token)
{
    return boost::asio::async_initiate<CompletionToken, void(error_code)>(
        detail::quit_connection_initiation(),
        token,
        &channel_.get(),
        &diag
    );
}

struct boost::mysql::detail::connection_access
{
    // Exposed for testing
    template <class Stream>
    static channel& get_channel(connection<Stream>& conn) noexcept
    {
        return conn.channel_.get();
    }
};

#endif
