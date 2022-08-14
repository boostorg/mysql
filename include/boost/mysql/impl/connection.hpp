//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_CONNECTION_HPP
#define BOOST_MYSQL_IMPL_CONNECTION_HPP

#pragma once

#include <boost/mysql/row.hpp>
#include <boost/mysql/connection.hpp>
#include <boost/mysql/prepared_statement.hpp>
#include <boost/mysql/resultset.hpp>
#include <boost/mysql/detail/network_algorithms/connect.hpp>
#include <boost/mysql/detail/network_algorithms/handshake.hpp>
#include <boost/mysql/detail/network_algorithms/execute_query.hpp>
#include <boost/mysql/detail/network_algorithms/prepare_statement.hpp>
#include <boost/mysql/detail/network_algorithms/execute_statement.hpp>
#include <boost/mysql/detail/network_algorithms/close_statement.hpp>
#include <boost/mysql/detail/network_algorithms/read_one_row.hpp>
#include <boost/mysql/detail/network_algorithms/read_some_rows.hpp>
#include <boost/mysql/detail/network_algorithms/read_all_rows.hpp>
#include <boost/mysql/detail/network_algorithms/quit_connection.hpp>
#include <boost/mysql/detail/network_algorithms/close_connection.hpp>
#include <boost/asio/buffer.hpp>
#include <utility>


// connect
template <class Stream>
template <class EndpointType>
void boost::mysql::connection<Stream>::connect(
    const EndpointType& endpoint,
    const connection_params& params,
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
    const connection_params& params
)
{
    detail::error_block blk;
    detail::connect(get_channel(), endpoint, params, blk.err, blk.info);
    blk.check();
}

template <class Stream>
template <
    class EndpointType,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken
>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code)
)
boost::mysql::connection<Stream>::async_connect(
    const EndpointType& endpoint,
    const connection_params& params,
    error_info& output_info,
    CompletionToken&& token
)
{
    output_info.clear();
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
    const connection_params& params,
    error_code& code,
    error_info& info
)
{
    detail::clear_errors(code, info);
    detail::handshake(get_channel(), params, code, info);
}

template <class Stream>
void boost::mysql::connection<Stream>::handshake(
    const connection_params& params
)
{
    detail::error_block blk;
    handshake(params, blk.err, blk.info);
    blk.check();
}

template <class Stream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code)
)
boost::mysql::connection<Stream>::async_handshake(
    const connection_params& params,
    error_info& output_info,
    CompletionToken&& token
)
{
    output_info.clear();
    return detail::async_handshake(
        get_channel(),
        params,
        output_info,
        std::forward<CompletionToken>(token)
    );
}

// Query
template <class Stream>
void boost::mysql::connection<Stream>::query(
    boost::string_view query_string,
    resultset& result,
    error_code& err,
    error_info& info
)
{
    detail::clear_errors(err, info);
    detail::execute_query(get_channel(), query_string, result, err, info);
}

template <class Stream>
void boost::mysql::connection<Stream>::query(
    boost::string_view query_string,
    resultset& result
)
{
    detail::error_block blk;
    detail::execute_query(get_channel(), query_string, result, blk.err, blk.info);
    blk.check();
}

template <class Stream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(
    void(::boost::mysql::error_code)
) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code)
)
boost::mysql::connection<Stream>::async_query(
    boost::string_view query_string,
    resultset& result,
    error_info& output_info,
    CompletionToken&& token
)
{
    output_info.clear();
    return detail::async_execute_query(
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
    boost::string_view statement,
    prepared_statement& output,
    error_code& err,
    error_info& info
)
{
    detail::clear_errors(err, info);
    detail::prepare_statement(get_channel(), statement, output, err, info);
}

template <class Stream>
void boost::mysql::connection<Stream>::prepare_statement(
    boost::string_view statement,
    prepared_statement& output
)
{
    detail::error_block blk;
    detail::prepare_statement(get_channel(), statement, output, blk.err, blk.info);
    blk.check();
}

template <class Stream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(
    void(::boost::mysql::error_code)
) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code)
)
boost::mysql::connection<Stream>::async_prepare_statement(
    boost::string_view statement,
    prepared_statement& output,
    error_info& output_info,
    CompletionToken&& token
)
{
    output_info.clear();
    return detail::async_prepare_statement(
        get_channel(),
        statement,
        output,
        output_info,
        std::forward<CompletionToken>(token)
    );
}


// Execute statement
template <class Stream>
template <class FieldViewFwdIterator>
void boost::mysql::connection<Stream>::execute_statement(
    const execute_params<FieldViewFwdIterator>& params,
    resultset& result,
    error_code& err,
    error_info& info
)
{
    detail::clear_errors(err, info);
    detail::execute_statement(
        get_channel(),
        params,
        result,
        err,
        info
    );
}

template <class Stream>
template <class FieldViewFwdIterator>
void boost::mysql::connection<Stream>::execute_statement(
    const execute_params<FieldViewFwdIterator>& params,
    resultset& result
)
{
    detail::error_block blk;
    detail::execute_statement(
        get_channel(),
        params,
        result,
        blk.err,
        blk.info
    );
    blk.check();
}


template <class Stream>
template <class FieldViewFwdIterator, BOOST_ASIO_COMPLETION_TOKEN_FOR(
    void(::boost::mysql::error_code)
) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code)
)
boost::mysql::connection<Stream>::async_execute_statement(
    const execute_params<FieldViewFwdIterator>& params,
    resultset& result,
    error_info& output_info,
    CompletionToken&& token
)
{
    output_info.clear();
    detail::async_execute_statement(
        get_channel(),
        params,
        result,
        output_info,
        std::forward<CompletionToken>(token)
    );
}

// Close statement
template <class Stream>
void boost::mysql::connection<Stream>::close_statement(
    const prepared_statement& stmt,
    error_code& code,
    error_info& info
)
{
    detail::clear_errors(code, info);
    detail::close_statement(get_channel(), stmt.id(), code, info);
}

template <class Stream>
void boost::mysql::connection<Stream>::close_statement(
    const prepared_statement& stmt
)
{
    detail::error_block blk;
    detail::close_statement(get_channel(), stmt.id(), blk.err, blk.info);
    blk.check();
}


template <class Stream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(
    void(::boost::mysql::error_code)
) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code)
)
boost::mysql::connection<Stream>::async_close_statement(
    const prepared_statement& stmt,
    error_info& output_info,
    CompletionToken&& token
)
{
    output_info.clear();
    return detail::async_close_statement(
        get_channel(),
        stmt.id(),
        std::forward<CompletionToken>(token),
        output_info
    );
}

// Read one row
template <class Stream>
bool boost::mysql::connection<Stream>::read_one_row(
    resultset& resultset,
	row& output,
    error_code& err,
    error_info& info
)
{
    detail::clear_errors(err, info);
    detail::read_one_row(get_channel(), resultset, output, err, info);
}


template <class Stream>
bool boost::mysql::connection<Stream>::read_one_row(
    resultset& resultset,
	row& output
)
{
    detail::error_block blk;
    bool res = detail::read_one_row(get_channel(), resultset, output, blk.err, blk.info);
    blk.check();
    return res;
}

template <class Stream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(
    void(::boost::mysql::error_code, bool)
) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code, bool)
)
boost::mysql::connection<Stream>::async_read_one_row(
    resultset& resultset,
	row& output,
    error_info& output_info,
    CompletionToken&& token
)
{
    output_info.clear();
    return detail::async_read_one_row(
        get_channel(),
        resultset,
        output,
        output_info,
        std::forward<CompletionToken>(token)
    );
}

// Read some rows
template <class Stream>
void boost::mysql::connection<Stream>::read_some_rows(
    resultset& resultset,
	rows_view& output,
    error_code& err,
    error_info& info
)
{
    detail::clear_errors(err, info);
    detail::read_some_rows(get_channel(), resultset, output, err, info);
}


template <class Stream>
void boost::mysql::connection<Stream>::read_some_rows(
    resultset& resultset,
	rows_view& output
)
{
    detail::error_block blk;
    detail::read_some_rows(get_channel(), resultset, output, blk.err, blk.info);
    blk.check();
}

template <class Stream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(
    void(::boost::mysql::error_code)
) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code)
)
boost::mysql::connection<Stream>::async_read_some_rows(
    resultset& resultset,
	rows_view& output,
    error_info& output_info,
    CompletionToken&& token
)
{
    output_info.clear();
    return detail::async_read_some_rows(
        get_channel(),
        resultset,
        output,
        output_info,
        std::forward<CompletionToken>(token)
    );
}

// Read all rows
template <class Stream>
void boost::mysql::connection<Stream>::read_all_rows(
    resultset& resultset,
	rows_view& output,
    error_code& err,
    error_info& info
)
{
    detail::clear_errors(err, info);
    detail::read_all_rows(get_channel(), resultset, output, err, info);
}


template <class Stream>
void boost::mysql::connection<Stream>::read_all_rows(
    resultset& resultset,
	rows_view& output
)
{
    detail::error_block blk;
    detail::read_all_rows(get_channel(), resultset, output, blk.err, blk.info);
    blk.check();
}

template <class Stream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(
    void(::boost::mysql::error_code)
) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code)
)
boost::mysql::connection<Stream>::async_read_all_rows(
    resultset& resultset,
	rows_view& output,
    error_info& output_info,
    CompletionToken&& token
)
{
    output_info.clear();
    return detail::async_read_all_rows(
        get_channel(),
        resultset,
        output,
        output_info,
        std::forward<CompletionToken>(token)
    );
}

// Close
template <class Stream>
void boost::mysql::connection<Stream>::close(
    error_code& err,
    error_info& info
)
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
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code)
)
boost::mysql::connection<Stream>::async_close(
    error_info& output_info,
    CompletionToken&& token
)
{
    output_info.clear();
    return detail::async_close_connection(
        get_channel(),
        std::forward<CompletionToken>(token),
        output_info
    );
}

template <class Stream>
void boost::mysql::connection<Stream>::quit(
    error_code& err,
    error_info& info
)
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
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code)
)
boost::mysql::connection<Stream>::async_quit(
    error_info& output_info,
    CompletionToken&& token
)
{
    output_info.clear();
    return detail::async_quit_connection(
        get_channel(),
        std::forward<CompletionToken>(token),
        output_info
    );
}

#endif
