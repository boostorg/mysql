//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_STATEMENT_HPP
#define BOOST_MYSQL_IMPL_STATEMENT_HPP

#pragma once

#include <boost/mysql/statement.hpp>

#include <boost/mysql/detail/network_algorithms/close_statement.hpp>
#include <boost/mysql/detail/network_algorithms/execute_statement.hpp>
#include <boost/mysql/detail/network_algorithms/start_statement_execution.hpp>

template <class Stream>
template <BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple, class>
void boost::mysql::statement<Stream>::execute(
    const FieldLikeTuple& params,
    resultset& result,
    error_code& err,
    error_info& info
)
{
    detail::clear_errors(err, info);
    detail::execute_statement(get_channel(), *this, params, result, err, info);
}

template <class Stream>
template <BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple, class>
void boost::mysql::statement<Stream>::execute(const FieldLikeTuple& params, resultset& result)
{
    detail::error_block blk;
    detail::execute_statement(get_channel(), *this, params, result, blk.err, blk.info);
    blk.check();
}

template <class Stream>
template <
    BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken,
    class>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::statement<Stream>::async_execute(
    FieldLikeTuple&& params,
    resultset& result,
    error_info& output_info,
    CompletionToken&& token
)
{
    return detail::async_execute_statement(
        get_channel(),
        *this,
        std::forward<FieldLikeTuple>(params),
        result,
        output_info,
        std::forward<CompletionToken>(token)
    );
}

template <class Stream>
template <BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple, class>
void boost::mysql::statement<Stream>::start_execution(
    const FieldLikeTuple& params,
    execution_state& result,
    error_code& err,
    error_info& info
)
{
    detail::clear_errors(err, info);
    detail::start_statement_execution(get_channel(), *this, params, result, err, info);
}

template <class Stream>
template <BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple, class>
void boost::mysql::statement<Stream>::start_execution(
    const FieldLikeTuple& params,
    execution_state& result
)
{
    detail::error_block blk;
    detail::start_statement_execution(get_channel(), *this, params, result, blk.err, blk.info);
    blk.check();
}

template <class Stream>
template <
    BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken,
    class>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::statement<Stream>::async_start_execution(
    FieldLikeTuple&& params,
    execution_state& result,
    error_info& output_info,
    CompletionToken&& token
)
{
    return detail::async_start_statement_execution(
        get_channel(),
        *this,
        std::forward<FieldLikeTuple>(params),
        result,
        output_info,
        std::forward<CompletionToken>(token)
    );
}

// Execute statement, with iterators
template <class Stream>
template <BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator>
void boost::mysql::statement<Stream>::start_execution(
    FieldViewFwdIterator params_first,
    FieldViewFwdIterator params_last,
    execution_state& result,
    error_code& err,
    error_info& info
)
{
    detail::clear_errors(err, info);
    detail::start_statement_execution(
        get_channel(),
        *this,
        params_first,
        params_last,
        result,
        err,
        info
    );
}

template <class Stream>
template <BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator>
void boost::mysql::statement<Stream>::start_execution(
    FieldViewFwdIterator params_first,
    FieldViewFwdIterator params_last,
    execution_state& result
)
{
    detail::error_block blk;
    detail::start_statement_execution(
        get_channel(),
        *this,
        params_first,
        params_last,
        result,
        blk.err,
        blk.info
    );
    blk.check();
}

template <class Stream>
template <
    BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::statement<Stream>::async_start_execution(
    FieldViewFwdIterator params_first,
    FieldViewFwdIterator params_last,
    execution_state& result,
    error_info& output_info,
    CompletionToken&& token
)
{
    return detail::async_start_statement_execution(
        get_channel(),
        *this,
        params_first,
        params_last,
        result,
        output_info,
        std::forward<CompletionToken>(token)
    );
}

// Close statement
template <class Stream>
void boost::mysql::statement<Stream>::close(error_code& code, error_info& info)
{
    detail::clear_errors(code, info);
    detail::close_statement(get_channel(), *this, code, info);
}

template <class Stream>
void boost::mysql::statement<Stream>::close()
{
    detail::error_block blk;
    detail::close_statement(get_channel(), *this, blk.err, blk.info);
    blk.check();
}

template <class Stream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::statement<Stream>::async_close(error_info& output_info, CompletionToken&& token)
{
    return detail::async_close_statement(
        get_channel(),
        *this,
        std::forward<CompletionToken>(token),
        output_info
    );
}

#endif
