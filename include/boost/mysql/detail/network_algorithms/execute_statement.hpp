//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_EXECUTE_STATEMENT_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_EXECUTE_STATEMENT_HPP

#include <boost/mysql/error_code.hpp>
#include <boost/mysql/resultset.hpp>
#include <boost/mysql/server_diagnostics.hpp>
#include <boost/mysql/statement_base.hpp>

#include <boost/mysql/detail/auxiliar/field_type_traits.hpp>
#include <boost/mysql/detail/channel/channel.hpp>

#include <boost/asio/async_result.hpp>

namespace boost {
namespace mysql {
namespace detail {

template <class Stream, BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple>
void execute_statement(
    channel<Stream>& channel,
    const statement_base& stmt,
    const FieldLikeTuple& params,
    resultset& output,
    error_code& err,
    server_diagnostics& diag
);

template <
    class Stream,
    BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
async_execute_statement(
    channel<Stream>& chan,
    const statement_base& stmt,
    FieldLikeTuple&& params,
    resultset& output,
    server_diagnostics& diag,
    CompletionToken&& token
);

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#include <boost/mysql/detail/network_algorithms/impl/execute_statement.hpp>

#endif
