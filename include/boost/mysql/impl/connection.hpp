//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_CONNECTION_HPP
#define BOOST_MYSQL_IMPL_CONNECTION_HPP

#include "boost/mysql/detail/network_algorithms/handshake.hpp"
#include "boost/mysql/detail/network_algorithms/execute_query.hpp"
#include "boost/mysql/detail/network_algorithms/prepare_statement.hpp"
#include "boost/mysql/detail/network_algorithms/quit_connection.hpp"
#include "boost/mysql/detail/network_algorithms/close_connection.hpp"
#include "boost/mysql/detail/network_algorithms/connect.hpp"
#include "boost/mysql/detail/auxiliar/check_completion_token.hpp"
#include <boost/asio/buffer.hpp>

template <typename Stream>
void boost::mysql::connection<Stream>::handshake(
    const connection_params& params,
    error_code& code,
    error_info& info
)
{
    detail::clear_errors(code, info);
    detail::handshake(channel_, params, code, info);
}

template <typename Stream>
void boost::mysql::connection<Stream>::handshake(
    const connection_params& params
)
{
    detail::error_block blk;
    handshake(params, blk.err, blk.info);
    blk.check();
}

template <typename Stream>
template <typename CompletionToken>
BOOST_MYSQL_INITFN_RESULT_TYPE(
    CompletionToken,
    typename boost::mysql::connection<Stream>::handshake_signature
)
boost::mysql::connection<Stream>::async_handshake(
    const connection_params& params,
    CompletionToken&& token,
    error_info* info
)
{
    detail::conditional_clear(info);

    // consider using: BOOST_ASIO_COMPLETION_TOKEN_FOR(sig)
//    detail::check_completion_token<CompletionToken, handshake_signature>();

    return detail::async_handshake(
        channel_,
        params,
        std::forward<CompletionToken>(token),
        info
    );
}

// Query
template <typename Stream>
boost::mysql::resultset<Stream> boost::mysql::connection<Stream>::query(
    std::string_view query_string,
    error_code& err,
    error_info& info
)
{
    detail::clear_errors(err, info);
    resultset<Stream> res;
    detail::execute_query(channel_, query_string, res, err, info);
    return res;
}

template <typename Stream>
boost::mysql::resultset<Stream> boost::mysql::connection<Stream>::query(
    std::string_view query_string
)
{
    resultset<Stream> res;
    detail::error_block blk;
    detail::execute_query(channel_, query_string, res, blk.err, blk.info);
    blk.check();
    return res;
}

template <typename Stream>
template <typename CompletionToken>
BOOST_MYSQL_INITFN_RESULT_TYPE(
    CompletionToken,
    typename boost::mysql::connection<Stream>::query_signature
)
boost::mysql::connection<Stream>::async_query(
    std::string_view query_string,
    CompletionToken&& token,
    error_info* info
)
{
    detail::conditional_clear(info);
    //detail::check_completion_token<CompletionToken, query_signature>();
    return detail::async_execute_query(
        channel_,
        query_string,
        std::forward<CompletionToken>(token),
        info
    );
}

template <typename Stream>
boost::mysql::prepared_statement<Stream> boost::mysql::connection<Stream>::prepare_statement(
    std::string_view statement,
    error_code& err,
    error_info& info
)
{
    mysql::prepared_statement<Stream> res;
    detail::clear_errors(err, info);
    detail::prepare_statement(channel_, statement, err, info, res);
    return res;
}

template <typename Stream>
boost::mysql::prepared_statement<Stream> boost::mysql::connection<Stream>::prepare_statement(
    std::string_view statement
)
{
    mysql::prepared_statement<Stream> res;
    detail::error_block blk;
    detail::prepare_statement(channel_, statement, blk.err, blk.info, res);
    blk.check();
    return res;
}

template <typename Stream>
template <typename CompletionToken>
BOOST_MYSQL_INITFN_RESULT_TYPE(
    CompletionToken,
    typename boost::mysql::connection<Stream>::prepare_statement_signature
)
boost::mysql::connection<Stream>::async_prepare_statement(
    std::string_view statement,
    CompletionToken&& token,
    error_info* info
)
{
    detail::conditional_clear(info);
    detail::check_completion_token<CompletionToken, prepare_statement_signature>();
    return detail::async_prepare_statement(
        channel_,
        statement,
        std::forward<CompletionToken>(token),
        info
    );
}

template <typename Stream>
void boost::mysql::connection<Stream>::quit(
    error_code& err,
    error_info& info
)
{
    detail::clear_errors(err, info);
    detail::quit_connection(channel_, err, info);
}

template <typename Stream>
void boost::mysql::connection<Stream>::quit()
{
    detail::error_block blk;
    detail::quit_connection(channel_, blk.err, blk.info);
    blk.check();
}

template <typename Stream>
template <typename CompletionToken>
BOOST_MYSQL_INITFN_RESULT_TYPE(
    CompletionToken,
    typename boost::mysql::connection<Stream>::quit_signature
)
boost::mysql::connection<Stream>::async_quit(
    CompletionToken&& token,
    error_info* info
)
{
    detail::conditional_clear(info);
    detail::check_completion_token<CompletionToken, quit_signature>();
    return detail::async_quit_connection(
        channel_,
        std::forward<CompletionToken>(token),
        info
    );
}

// socket_connection: connect
template <typename Stream>
void boost::mysql::socket_connection<Stream>::connect(
    const endpoint_type& endpoint,
    const connection_params& params,
    error_code& ec,
    error_info& info
)
{
    detail::clear_errors(ec, info);
    detail::connect(this->get_channel(), endpoint, params, ec, info);
}

template <typename Stream>
void boost::mysql::socket_connection<Stream>::connect(
    const endpoint_type& endpoint,
    const connection_params& params
)
{
    detail::error_block blk;
    detail::connect(this->get_channel(), endpoint, params, blk.err, blk.info);
    blk.check();
}

template <typename Stream>
template <typename CompletionToken>
BOOST_MYSQL_INITFN_RESULT_TYPE(
    CompletionToken,
    typename boost::mysql::socket_connection<Stream>::connect_signature
)
boost::mysql::socket_connection<Stream>::async_connect(
    const endpoint_type& endpoint,
    const connection_params& params,
    CompletionToken&& token,
    error_info* output_info
)
{
    detail::conditional_clear(output_info);
//    detail::check_completion_token<CompletionToken, connect_signature>();
    return detail::async_connect(
        this->get_channel(),
        endpoint,
        params,
        std::forward<CompletionToken>(token),
        output_info
    );
}

template <typename Stream>
void boost::mysql::socket_connection<Stream>::close(
    error_code& err,
    error_info& info
)
{
    detail::clear_errors(err, info);
    detail::close_connection(this->get_channel(), err, info);
}

template <typename Stream>
void boost::mysql::socket_connection<Stream>::close()
{
    detail::error_block blk;
    detail::close_connection(this->get_channel(), blk.err, blk.info);
    blk.check();
}

template <typename Stream>
template <typename CompletionToken>
BOOST_MYSQL_INITFN_RESULT_TYPE(
    CompletionToken,
    typename boost::mysql::socket_connection<Stream>::close_signature
)
boost::mysql::socket_connection<Stream>::async_close(
    CompletionToken&& token,
    error_info* info
)
{
    detail::conditional_clear(info);
    detail::check_completion_token<CompletionToken, close_signature>();
    return detail::async_close_connection(
        this->get_channel(),
        std::forward<CompletionToken>(token),
        info
    );
}

#endif
