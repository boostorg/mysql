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
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/row.hpp>
#include <boost/mysql/statement_base.hpp>

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

#include <utility>

// connect
template <class Stream>
template <class EndpointType>
void boost::mysql::connection<Stream>::connect(
    const EndpointType& endpoint,
    const handshake_params& params,
    error_code& ec,
    error_info& info
)
{
    detail::clear_errors(ec, info);
    detail::connect(get_channel(), endpoint, params, ec, info);
}

template <class Stream>
template <class EndpointType>
void boost::mysql::connection<Stream>::connect(
    const EndpointType& endpoint,
    const handshake_params& params
)
{
    detail::error_block blk;
    detail::connect(get_channel(), endpoint, params, blk.err, blk.info);
    blk.check();
}

template <class Stream>
template <
    class EndpointType,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::connection<Stream>::async_connect(
    const EndpointType& endpoint,
    const handshake_params& params,
    error_info& output_info,
    CompletionToken&& token
)
{
    return detail::async_connect(
        get_channel(),
        endpoint,
        params,
        output_info,
        std::forward<CompletionToken>(token)
    );
}

// handshake
template <class Stream>
void boost::mysql::connection<Stream>::handshake(
    const handshake_params& params,
    error_code& code,
    error_info& info
)
{
    detail::clear_errors(code, info);
    detail::handshake(get_channel(), params, code, info);
}

template <class Stream>
void boost::mysql::connection<Stream>::handshake(const handshake_params& params)
{
    detail::error_block blk;
    handshake(params, blk.err, blk.info);
    blk.check();
}

template <class Stream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::connection<Stream>::async_handshake(
    const handshake_params& params,
    error_info& output_info,
    CompletionToken&& token
)
{
    return detail::async_handshake(
        get_channel(),
        params,
        output_info,
        std::forward<CompletionToken>(token)
    );
}

template <class Stream>
void boost::mysql::connection<Stream>::start_query(
    boost::string_view query_string,
    execution_state& result,
    error_code& err,
    error_info& info
)
{
    detail::clear_errors(err, info);
    detail::start_query(get_channel(), query_string, result, err, info);
}

template <class Stream>
void boost::mysql::connection<Stream>::start_query(
    boost::string_view query_string,
    execution_state& result
)
{
    detail::error_block blk;
    detail::start_query(get_channel(), query_string, result, blk.err, blk.info);
    blk.check();
}

template <class Stream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::connection<Stream>::async_start_query(
    boost::string_view query_string,
    execution_state& result,
    error_info& output_info,
    CompletionToken&& token
)
{
    return detail::async_start_query(
        get_channel(),
        query_string,
        result,
        output_info,
        std::forward<CompletionToken>(token)
    );
}

template <class Stream>
void boost::mysql::connection<Stream>::query(
    boost::string_view query_string,
    resultset& result,
    error_code& err,
    error_info& info
)
{
    detail::clear_errors(err, info);
    detail::query(get_channel(), query_string, result, err, info);
}

template <class Stream>
void boost::mysql::connection<Stream>::query(boost::string_view query_string, resultset& result)
{
    detail::error_block blk;
    detail::query(get_channel(), query_string, result, blk.err, blk.info);
    blk.check();
}

template <class Stream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::connection<Stream>::async_query(
    boost::string_view query_string,
    resultset& result,
    error_info& output_info,
    CompletionToken&& token
)
{
    return detail::async_query(
        get_channel(),
        query_string,
        result,
        output_info,
        std::forward<CompletionToken>(token)
    );
}

// Prepare statement
template <class Stream>
void boost::mysql::connection<Stream>::prepare_statement(
    boost::string_view stmt,
    statement<Stream>& output,
    error_code& err,
    error_info& info
)
{
    detail::clear_errors(err, info);
    detail::prepare_statement(get_channel(), stmt, output, err, info);
}

template <class Stream>
void boost::mysql::connection<Stream>::prepare_statement(
    boost::string_view stmt,
    statement<Stream>& output
)
{
    detail::error_block blk;
    detail::prepare_statement(get_channel(), stmt, output, blk.err, blk.info);
    blk.check();
}

template <class Stream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::connection<Stream>::async_prepare_statement(
    boost::string_view stmt,
    statement<Stream>& output,
    error_info& output_info,
    CompletionToken&& token
)
{
    return detail::async_prepare_statement(
        get_channel(),
        stmt,
        output,
        output_info,
        std::forward<CompletionToken>(token)
    );
}

// Close
template <class Stream>
void boost::mysql::connection<Stream>::close(error_code& err, error_info& info)
{
    detail::clear_errors(err, info);
    detail::close_connection(get_channel(), err, info);
}

template <class Stream>
void boost::mysql::connection<Stream>::close()
{
    detail::error_block blk;
    detail::close_connection(get_channel(), blk.err, blk.info);
    blk.check();
}

template <class Stream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::connection<Stream>::async_close(error_info& output_info, CompletionToken&& token)
{
    return detail::async_close_connection(
        get_channel(),
        std::forward<CompletionToken>(token),
        output_info
    );
}

template <class Stream>
void boost::mysql::connection<Stream>::quit(error_code& err, error_info& info)
{
    detail::clear_errors(err, info);
    detail::quit_connection(get_channel(), err, info);
}

template <class Stream>
void boost::mysql::connection<Stream>::quit()
{
    detail::error_block blk;
    detail::quit_connection(get_channel(), blk.err, blk.info);
    blk.check();
}

template <class Stream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::connection<Stream>::async_quit(error_info& output_info, CompletionToken&& token)
{
    return detail::async_quit_connection(
        get_channel(),
        std::forward<CompletionToken>(token),
        output_info
    );
}

template <class Stream>
boost::mysql::row_view boost::mysql::connection<Stream>::read_one_row(
    execution_state& st,
    error_code& err,
    error_info& info
)
{
    detail::clear_errors(err, info);
    return detail::read_one_row(get_channel(), st, err, info);
}

template <class Stream>
boost::mysql::row_view boost::mysql::connection<Stream>::read_one_row(execution_state& st)
{
    detail::error_block blk;
    row_view res = detail::read_one_row(get_channel(), st, blk.err, blk.info);
    blk.check();
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
    error_info& output_info,
    CompletionToken&& token
)
{
    return detail::async_read_one_row(
        get_channel(),
        st,
        output_info,
        std::forward<CompletionToken>(token)
    );
}

template <class Stream>
boost::mysql::rows_view boost::mysql::connection<Stream>::read_some_rows(
    execution_state& st,
    error_code& err,
    error_info& info
)
{
    detail::clear_errors(err, info);
    return detail::read_some_rows(get_channel(), st, err, info);
}

template <class Stream>
boost::mysql::rows_view boost::mysql::connection<Stream>::read_some_rows(execution_state& st)
{
    detail::error_block blk;
    rows_view res = detail::read_some_rows(get_channel(), st, blk.err, blk.info);
    blk.check();
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
    error_info& output_info,
    CompletionToken&& token
)
{
    return detail::async_read_some_rows(
        get_channel(),
        st,
        output_info,
        std::forward<CompletionToken>(token)
    );
}

#endif
