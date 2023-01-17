//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_CONNECTION_HPP
#define BOOST_MYSQL_IMPL_CONNECTION_HPP

#include <boost/mysql/server_diagnostics.hpp>
#pragma once

#include <boost/mysql/connection.hpp>
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/row.hpp>
#include <boost/mysql/statement_base.hpp>

#include <boost/mysql/detail/auxiliar/error_helpers.hpp>
#include <boost/mysql/detail/network_algorithms/close_connection.hpp>
#include <boost/mysql/detail/network_algorithms/connect.hpp>
#include <boost/mysql/detail/network_algorithms/handshake.hpp>
#include <boost/mysql/detail/network_algorithms/prepare_statement.hpp>
#include <boost/mysql/detail/network_algorithms/query.hpp>
#include <boost/mysql/detail/network_algorithms/quit_connection.hpp>
#include <boost/mysql/detail/network_algorithms/read_one_row.hpp>
#include <boost/mysql/detail/network_algorithms/read_some_rows.hpp>
#include <boost/mysql/detail/network_algorithms/start_query.hpp>

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
    server_diagnostics& diag
)
{
    detail::clear_errors(ec, diag);
    detail::connect(get_channel(), endpoint, params, ec, diag);
}

template <class Stream>
template <class EndpointType>
void boost::mysql::connection<Stream>::connect(
    const EndpointType& endpoint,
    const handshake_params& params
)
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
    server_diagnostics& diag,
    CompletionToken&& token
)
{
    return detail::async_connect(
        get_channel(),
        endpoint,
        params,
        diag,
        std::forward<CompletionToken>(token)
    );
}

// handshake
template <class Stream>
void boost::mysql::connection<Stream>::handshake(
    const handshake_params& params,
    error_code& code,
    server_diagnostics& diag
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
    server_diagnostics& diag,
    CompletionToken&& token
)
{
    return detail::async_handshake(
        get_channel(),
        params,
        diag,
        std::forward<CompletionToken>(token)
    );
}

template <class Stream>
void boost::mysql::connection<Stream>::start_query(
    string_view query_string,
    execution_state& result,
    error_code& err,
    server_diagnostics& diag
)
{
    detail::clear_errors(err, diag);
    detail::start_query(get_channel(), query_string, result, err, diag);
}

template <class Stream>
void boost::mysql::connection<Stream>::start_query(
    string_view query_string,
    execution_state& result
)
{
    detail::error_block blk;
    detail::start_query(get_channel(), query_string, result, blk.err, blk.diag);
    blk.check(BOOST_CURRENT_LOCATION);
}

template <class Stream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::connection<Stream>::async_start_query(
    string_view query_string,
    execution_state& result,
    server_diagnostics& diag,
    CompletionToken&& token
)
{
    return detail::async_start_query(
        get_channel(),
        query_string,
        result,
        diag,
        std::forward<CompletionToken>(token)
    );
}

template <class Stream>
void boost::mysql::connection<Stream>::query(
    string_view query_string,
    resultset& result,
    error_code& err,
    server_diagnostics& diag
)
{
    detail::clear_errors(err, diag);
    detail::query(get_channel(), query_string, result, err, diag);
}

template <class Stream>
void boost::mysql::connection<Stream>::query(string_view query_string, resultset& result)
{
    detail::error_block blk;
    detail::query(get_channel(), query_string, result, blk.err, blk.diag);
    blk.check(BOOST_CURRENT_LOCATION);
}

template <class Stream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::connection<Stream>::async_query(
    string_view query_string,
    resultset& result,
    server_diagnostics& diag,
    CompletionToken&& token
)
{
    return detail::async_query(
        get_channel(),
        query_string,
        result,
        diag,
        std::forward<CompletionToken>(token)
    );
}

// Prepare statement
template <class Stream>
void boost::mysql::connection<Stream>::prepare_statement(
    string_view stmt,
    statement<Stream>& output,
    error_code& err,
    server_diagnostics& diag
)
{
    detail::clear_errors(err, diag);
    detail::prepare_statement(get_channel(), stmt, output, err, diag);
}

template <class Stream>
void boost::mysql::connection<Stream>::prepare_statement(
    string_view stmt,
    statement<Stream>& output
)
{
    detail::error_block blk;
    detail::prepare_statement(get_channel(), stmt, output, blk.err, blk.diag);
    blk.check(BOOST_CURRENT_LOCATION);
}

template <class Stream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::connection<Stream>::async_prepare_statement(
    string_view stmt,
    statement<Stream>& output,
    server_diagnostics& diag,
    CompletionToken&& token
)
{
    return detail::async_prepare_statement(
        get_channel(),
        stmt,
        output,
        diag,
        std::forward<CompletionToken>(token)
    );
}

// Close
template <class Stream>
void boost::mysql::connection<Stream>::close(error_code& err, server_diagnostics& diag)
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
boost::mysql::connection<Stream>::async_close(server_diagnostics& diag, CompletionToken&& token)
{
    return detail::async_close_connection(
        get_channel(),
        std::forward<CompletionToken>(token),
        diag
    );
}

template <class Stream>
void boost::mysql::connection<Stream>::quit(error_code& err, server_diagnostics& diag)
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
boost::mysql::connection<Stream>::async_quit(server_diagnostics& diag, CompletionToken&& token)
{
    return detail::async_quit_connection(get_channel(), std::forward<CompletionToken>(token), diag);
}

template <class Stream>
boost::mysql::row_view boost::mysql::connection<Stream>::read_one_row(
    execution_state& st,
    error_code& err,
    server_diagnostics& diag
)
{
    detail::clear_errors(err, diag);
    return detail::read_one_row(get_channel(), st, err, diag);
}

template <class Stream>
boost::mysql::row_view boost::mysql::connection<Stream>::read_one_row(execution_state& st)
{
    detail::error_block blk;
    row_view res = detail::read_one_row(get_channel(), st, blk.err, blk.diag);
    blk.check(BOOST_CURRENT_LOCATION);
    return res;
}

template <class Stream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::row_view)
) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code, boost::mysql::row_view)
)
boost::mysql::connection<Stream>::async_read_one_row(
    execution_state& st,
    server_diagnostics& diag,
    CompletionToken&& token
)
{
    return detail::async_read_one_row(
        get_channel(),
        st,
        diag,
        std::forward<CompletionToken>(token)
    );
}

template <class Stream>
boost::mysql::rows_view boost::mysql::connection<Stream>::read_some_rows(
    execution_state& st,
    error_code& err,
    server_diagnostics& diag
)
{
    detail::clear_errors(err, diag);
    return detail::read_some_rows(get_channel(), st, err, diag);
}

template <class Stream>
boost::mysql::rows_view boost::mysql::connection<Stream>::read_some_rows(execution_state& st)
{
    detail::error_block blk;
    rows_view res = detail::read_some_rows(get_channel(), st, blk.err, blk.diag);
    blk.check(BOOST_CURRENT_LOCATION);
    return res;
}

template <class Stream>
template <
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::rows_view))
        CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code, boost::mysql::rows_view)
)
boost::mysql::connection<Stream>::async_read_some_rows(
    execution_state& st,
    server_diagnostics& diag,
    CompletionToken&& token
)
{
    return detail::async_read_some_rows(
        get_channel(),
        st,
        diag,
        std::forward<CompletionToken>(token)
    );
}

#endif
