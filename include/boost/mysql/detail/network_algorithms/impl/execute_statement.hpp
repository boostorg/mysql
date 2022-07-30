//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_STATEMENT_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_STATEMENT_HPP

#pragma once

#include <boost/mysql/resultset.hpp>
#include <boost/mysql/execute_params.hpp>
#include <boost/mysql/detail/network_algorithms/execute_statement.hpp>
#include <boost/mysql/detail/protocol/binary_deserialization.hpp>
#include <boost/mysql/detail/protocol/prepared_statement_messages.hpp>
#include <boost/mysql/detail/network_algorithms/execute_generic.hpp>

namespace boost {
namespace mysql {
namespace detail {

template <class ValueForwardIterator>
com_stmt_execute_packet<ValueForwardIterator> make_stmt_execute_packet(
    const execute_params<ValueForwardIterator>& params
)
{
    return com_stmt_execute_packet<ValueForwardIterator> {
        params.statement_id(),
        std::uint8_t(0),  // flags
        std::uint32_t(1), // iteration count
        std::uint8_t(1),  // new params flag: set
        params.first(),
        params.last()
    };
}


} // detail
} // mysql
} // boost

template <class Stream, class ValueForwardIterator>
void boost::mysql::detail::execute_statement(
    channel<Stream>& chan,
    const execute_params<ValueForwardIterator>& params,
    resultset& output,
    error_code& err,
    error_info& info
)
{
    execute_generic(
        &deserialize_binary_row,
        chan,
        make_stmt_execute_packet(params),
        output,
        err,
        info
    );
}

template <class Stream, class ValueForwardIterator, class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code)
)
boost::mysql::detail::async_execute_statement(
    channel<Stream>& chan,
    const execute_params<ValueForwardIterator>& params,
    resultset& output,
    error_info& info,
    CompletionToken&& token
)
{
    return async_execute_generic(
        &deserialize_binary_row,
        chan,
        make_stmt_execute_packet(params),
        output,
        info,
        std::forward<CompletionToken>(token)
    );
}



#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_STATEMENT_HPP_ */
