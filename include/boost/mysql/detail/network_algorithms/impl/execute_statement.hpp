//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_STATEMENT_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_STATEMENT_HPP

#pragma once

#include <boost/mysql/detail/network_algorithms/execute_statement.hpp>
#include <boost/mysql/detail/protocol/binary_deserialization.hpp>
#include <boost/mysql/detail/protocol/prepared_statement_messages.hpp>
#include <boost/mysql/detail/network_algorithms/execute_generic.hpp>

namespace boost {
namespace mysql {
namespace detail {

template <class ValueForwardIterator>
com_stmt_execute_packet<ValueForwardIterator> make_stmt_execute_packet(
    std::uint32_t statement_id,
    ValueForwardIterator params_begin,
    ValueForwardIterator params_end
)
{
    return com_stmt_execute_packet<ValueForwardIterator> {
        statement_id,
        std::uint8_t(0),  // flags
        std::uint32_t(1), // iteration count
        std::uint8_t(1),  // new params flag: set
        params_begin,
        params_end
    };
}

} // detail
} // mysql
} // boost

template <class Stream, class ValueForwardIterator>
void boost::mysql::detail::execute_statement(
    channel<Stream>& chan,
    std::uint32_t statement_id,
    ValueForwardIterator params_begin,
    ValueForwardIterator params_end,
    resultset<Stream>& output,
    error_code& err,
    error_info& info
)
{
    execute_generic(
        &deserialize_binary_row,
        chan,
        make_stmt_execute_packet(statement_id, params_begin, params_end),
        output,
        err,
        info
    );
}

template <class Stream, class ValueForwardIterator, class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code, boost::mysql::resultset<Stream>)
)
boost::mysql::detail::async_execute_statement(
    channel<Stream>& chan,
    std::uint32_t statement_id,
    ValueForwardIterator params_begin,
    ValueForwardIterator params_end,
    CompletionToken&& token,
    error_info& info
)
{
    return async_execute_generic(
        &deserialize_binary_row,
        chan,
        make_stmt_execute_packet(statement_id, params_begin, params_end),
        std::forward<CompletionToken>(token),
        info
    );
}



#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_STATEMENT_HPP_ */
