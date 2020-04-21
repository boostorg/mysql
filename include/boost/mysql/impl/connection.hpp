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
#include "boost/mysql/detail/auxiliar/check_completion_token.hpp"
#include <boost/asio/buffer.hpp>

template <typename Stream>
void boost::mysql::connection<Stream>::handshake(
    const connection_params& params,
    error_code& code,
    error_info& info
)
{
    code.clear();
    info.clear();
    detail::hanshake(channel_, params, code, info);
    // TODO: should we close() the stream in case of error?
}

template <typename Stream>
void boost::mysql::connection<Stream>::handshake(
    const connection_params& params
)
{
    error_code code;
    error_info info;
    handshake(params, code, info);
    detail::check_error_code(code, info);
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
    detail::check_completion_token<CompletionToken, handshake_signature>();
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
    err.clear();
    info.clear();
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
    error_code err;
    error_info info;
    detail::execute_query(channel_, query_string, res, err, info);
    detail::check_error_code(err, info);
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
    detail::check_completion_token<CompletionToken, query_signature>();
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
    err.clear();
    info.clear();
    detail::prepare_statement(channel_, statement, err, info, res);
    return res;
}

template <typename Stream>
boost::mysql::prepared_statement<Stream> boost::mysql::connection<Stream>::prepare_statement(
    std::string_view statement
)
{
    mysql::prepared_statement<Stream> res;
    error_code err;
    error_info info;
    detail::prepare_statement(channel_, statement, err, info, res);
    detail::check_error_code(err, info);
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
    err.clear();
    info.clear();
    detail::quit_connection(channel_, err, info);
}

template <typename Stream>
void boost::mysql::connection<Stream>::quit()
{
    error_code err;
    error_info info;
    detail::quit_connection(channel_, err, info);
    detail::check_error_code(err, info);
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
    return detail::async_quit_connection(
        channel_,
        std::forward<CompletionToken>(token),
        info
    );
}


#endif
