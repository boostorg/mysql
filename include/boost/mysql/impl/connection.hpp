//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_CONNECTION_HPP
#define BOOST_MYSQL_IMPL_CONNECTION_HPP

#pragma once

#include <boost/mysql/connection.hpp>
#include <boost/mysql/detail/network_algorithms/connect.hpp>
#include <boost/mysql/detail/network_algorithms/handshake.hpp>
#include <boost/mysql/detail/network_algorithms/execute_query.hpp>
#include <boost/mysql/detail/network_algorithms/prepare_statement.hpp>
#include <boost/mysql/detail/network_algorithms/quit_connection.hpp>
#include <boost/mysql/detail/network_algorithms/close_connection.hpp>
#include <boost/asio/buffer.hpp>


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
    detail::connect(this->get_channel(), endpoint, params, ec, info);
}

template <class Stream>
template <class EndpointType>
void boost::mysql::connection<Stream>::connect(
    const EndpointType& endpoint,
    const connection_params& params
)
{
    detail::error_block blk;
    detail::connect(this->get_channel(), endpoint, params, blk.err, blk.info);
    blk.check();
}

template <class Stream>
template <
    class EndpointType,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::mysql::error_code)) CompletionToken
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
        this->get_channel(),
        endpoint,
        params,
        std::forward<CompletionToken>(token),
        output_info
    );
}

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
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::mysql::error_code)) CompletionToken>
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
        std::forward<CompletionToken>(token),
        output_info
    );
}

// Query
template <class Stream>
boost::mysql::resultset<Stream> boost::mysql::connection<Stream>::query(
    boost::string_view query_string,
    error_code& err,
    error_info& info
)
{
    detail::clear_errors(err, info);
    resultset<Stream> res;
    detail::execute_query(get_channel(), query_string, res, err, info);
    return res;
}

template <class Stream>
boost::mysql::resultset<Stream> boost::mysql::connection<Stream>::query(
    boost::string_view query_string
)
{
    resultset<Stream> res;
    detail::error_block blk;
    detail::execute_query(get_channel(), query_string, res, blk.err, blk.info);
    blk.check();
    return res;
}

template <class Stream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(
    void(boost::mysql::error_code, boost::mysql::resultset<Stream>)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code, boost::mysql::resultset<Stream>)
)
boost::mysql::connection<Stream>::async_query(
    boost::string_view query_string,
    error_info& output_info,
    CompletionToken&& token
)
{
    output_info.clear();
    return detail::async_execute_query(
        get_channel(),
        query_string,
        std::forward<CompletionToken>(token),
        output_info
    );
}

template <class Stream>
boost::mysql::prepared_statement<Stream> boost::mysql::connection<Stream>::prepare_statement(
    boost::string_view statement,
    error_code& err,
    error_info& info
)
{
    mysql::prepared_statement<Stream> res;
    detail::clear_errors(err, info);
    detail::prepare_statement(get_channel(), statement, err, info, res);
    return res;
}

template <class Stream>
boost::mysql::prepared_statement<Stream> boost::mysql::connection<Stream>::prepare_statement(
    boost::string_view statement
)
{
    mysql::prepared_statement<Stream> res;
    detail::error_block blk;
    detail::prepare_statement(get_channel(), statement, blk.err, blk.info, res);
    blk.check();
    return res;
}

template <class Stream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(
    void(boost::mysql::error_code, boost::mysql::prepared_statement<Stream>)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code, boost::mysql::prepared_statement<Stream>)
)
boost::mysql::connection<Stream>::async_prepare_statement(
    boost::string_view statement,
    error_info& output_info,
    CompletionToken&& token
)
{
    output_info.clear();
    return detail::async_prepare_statement(
        get_channel(),
        statement,
        std::forward<CompletionToken>(token),
        output_info
    );
}


template <class Stream>
void boost::mysql::connection<Stream>::close(
    error_code& err,
    error_info& info
)
{
    detail::clear_errors(err, info);
    detail::close_connection(this->get_channel(), err, info);
}

template <class Stream>
void boost::mysql::connection<Stream>::close()
{
    detail::error_block blk;
    detail::close_connection(this->get_channel(), blk.err, blk.info);
    blk.check();
}

template <class Stream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::mysql::error_code)) CompletionToken>
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
        this->get_channel(),
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
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::mysql::error_code)) CompletionToken>
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
