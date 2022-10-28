//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_STATEMENT_HPP
#define BOOST_MYSQL_IMPL_STATEMENT_HPP

#pragma once

#include <boost/mysql/detail/network_algorithms/close_statement.hpp>
#include <boost/mysql/detail/network_algorithms/execute_statement.hpp>
#include <boost/mysql/statement.hpp>

// Execute statement, with tuple
template <class Stream>
template <BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple, class>
void boost::mysql::statement<Stream>::execute(
    const FieldLikeTuple& params,
    const execute_options& opts,
    resultset<Stream>& result,
    error_code& err,
    error_info& info
)
{
    detail::clear_errors(err, info);
    detail::execute_statement(get_channel(), *this, params, opts, result, err, info);
}

template <class Stream>
template <BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple, class>
void boost::mysql::statement<Stream>::execute(
    const FieldLikeTuple& params,
    const execute_options& opts,
    resultset<Stream>& result
)
{
    detail::error_block blk;
    detail::execute_statement(get_channel(), *this, params, opts, result, blk.err, blk.info);
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
    const execute_options& opts,
    resultset<Stream>& result,
    error_info& output_info,
    CompletionToken&& token
)
{
    return detail::async_execute_statement(
        get_channel(),
        *this,
        std::forward<FieldLikeTuple>(params),
        opts,
        result,
        output_info,
        std::forward<CompletionToken>(token)
    );
}

// Execute statement, with iterators
template <class Stream>
template <BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator>
void boost::mysql::statement<Stream>::execute(
    FieldViewFwdIterator params_first,
    FieldViewFwdIterator params_last,
    const execute_options& opts,
    resultset<Stream>& result,
    error_code& err,
    error_info& info
)
{
    detail::clear_errors(err, info);
    detail::execute_statement(
        get_channel(),
        *this,
        params_first,
        params_last,
        opts,
        result,
        err,
        info
    );
}

template <class Stream>
template <BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator>
void boost::mysql::statement<Stream>::execute(
    FieldViewFwdIterator params_first,
    FieldViewFwdIterator params_last,
    const execute_options& opts,
    resultset<Stream>& result
)
{
    detail::error_block blk;
    detail::execute_statement(
        get_channel(),
        *this,
        params_first,
        params_last,
        opts,
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
boost::mysql::statement<Stream>::async_execute(
    FieldViewFwdIterator params_first,
    FieldViewFwdIterator params_last,
    const execute_options& opts,
    resultset<Stream>& result,
    error_info& output_info,
    CompletionToken&& token
)
{
    return detail::async_execute_statement(
        get_channel(),
        *this,
        params_first,
        params_last,
        opts,
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
