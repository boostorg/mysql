//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_RESULTSET_HPP
#define BOOST_MYSQL_IMPL_RESULTSET_HPP

#pragma once

#include <boost/mysql/resultset.hpp>
#include <boost/mysql/detail/network_algorithms/read_one_row.hpp>
#include <boost/mysql/detail/network_algorithms/read_some_rows.hpp>
#include <boost/mysql/detail/network_algorithms/read_all_rows.hpp>

// Read one row
template <class Stream>
bool boost::mysql::resultset<Stream>::read_one(
	row_view& output,
    error_code& err,
    error_info& info
)
{
    detail::clear_errors(err, info);
    detail::read_one_row(get_channel(), *this, output, err, info);
}


template <class Stream>
bool boost::mysql::resultset<Stream>::read_one(
	row_view& output
)
{
    detail::error_block blk;
    bool res = detail::read_one_row(get_channel(), *this, output, blk.err, blk.info);
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
boost::mysql::resultset<Stream>::async_read_one(
	row_view& output,
    error_info& output_info,
    CompletionToken&& token
)
{
    output_info.clear();
    return detail::async_read_one_row(
        get_channel(),
        *this,
        output,
        output_info,
        std::forward<CompletionToken>(token)
    );
}

// Read some rows
template <class Stream>
void boost::mysql::resultset<Stream>::read_some(
	rows_view& output,
    error_code& err,
    error_info& info
)
{
    detail::clear_errors(err, info);
    detail::read_some_rows(get_channel(), *this, output, err, info);
}


template <class Stream>
void boost::mysql::resultset<Stream>::read_some(
	rows_view& output
)
{
    detail::error_block blk;
    detail::read_some_rows(get_channel(), *this, output, blk.err, blk.info);
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
boost::mysql::resultset<Stream>::async_read_some(
	rows_view& output,
    error_info& output_info,
    CompletionToken&& token
)
{
    output_info.clear();
    return detail::async_read_some_rows(
        get_channel(),
        *this,
        output,
        output_info,
        std::forward<CompletionToken>(token)
    );
}

// Read all rows
template <class Stream>
void boost::mysql::resultset<Stream>::read_all(
	rows_view& output,
    error_code& err,
    error_info& info
)
{
    detail::clear_errors(err, info);
    detail::read_all_rows(get_channel(), *this, output, err, info);
}


template <class Stream>
void boost::mysql::resultset<Stream>::read_all(
	rows_view& output
)
{
    detail::error_block blk;
    detail::read_all_rows(get_channel(), *this, output, blk.err, blk.info);
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
boost::mysql::resultset<Stream>::async_read_all(
	rows_view& output,
    error_info& output_info,
    CompletionToken&& token
)
{
    output_info.clear();
    return detail::async_read_all_rows(
        get_channel(),
        *this,
        output,
        output_info,
        std::forward<CompletionToken>(token)
    );
}

#endif
