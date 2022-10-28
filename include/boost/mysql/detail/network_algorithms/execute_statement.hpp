//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_EXECUTE_STATEMENT_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_EXECUTE_STATEMENT_HPP

#include <boost/mysql/detail/auxiliar/field_type_traits.hpp>
#include <boost/mysql/detail/channel/channel.hpp>
#include <boost/mysql/error.hpp>
#include <boost/mysql/execute_options.hpp>
#include <boost/mysql/resultset_base.hpp>
#include <boost/mysql/statement_base.hpp>

namespace boost {
namespace mysql {
namespace detail {

template <class Stream, BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator>
void execute_statement(
    channel<Stream>& channel,
    const statement_base& stmt,
    FieldViewFwdIterator params_first,
    FieldViewFwdIterator params_last,
    const execute_options& opts,
    resultset_base& output,
    error_code& err,
    error_info& info
);

template <
    class Stream,
    BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator,
    class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
async_execute_statement(
    channel<Stream>& chan,
    const statement_base& stmt,
    FieldViewFwdIterator params_first,
    FieldViewFwdIterator params_last,
    const execute_options& opts,
    resultset_base& output,
    error_info& info,
    CompletionToken&& token
);

template <class Stream, BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple>
void execute_statement(
    channel<Stream>& channel,
    const statement_base& stmt,
    const FieldLikeTuple& params,
    const execute_options& opts,
    resultset_base& output,
    error_code& err,
    error_info& info
);

template <class Stream, BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple, class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
async_execute_statement(
    channel<Stream>& chan,
    const statement_base& stmt,
    FieldLikeTuple&& params,
    const execute_options& opts,
    resultset_base& output,
    error_info& info,
    CompletionToken&& token
);

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#include <boost/mysql/detail/network_algorithms/impl/execute_statement.hpp>

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_EXECUTE_STATEMENT_HPP_ */
