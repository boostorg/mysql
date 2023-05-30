//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_CLOSE_STATEMENT_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_CLOSE_STATEMENT_HPP

#pragma once

#include <boost/mysql/detail/channel/channel.hpp>
#include <boost/mysql/detail/network_algorithms/close_statement.hpp>
#include <boost/mysql/detail/protocol/prepared_statement_messages.hpp>

#include <boost/asio/coroutine.hpp>

void boost::mysql::detail::
    close_statement_impl(channel& chan, const statement& stmt, error_code& code, diagnostics&)
{
    // Serialize the close message
    chan.serialize(com_stmt_close_packet{stmt.id()}, chan.reset_sequence_number());

    // Send it. No response is sent back
    chan.write(code);
}

void boost::mysql::detail::async_close_statement_impl(
    channel& chan,
    const statement& stmt,
    diagnostics& diag,
    asio::any_completion_handler<void(error_code)> handler
)
{
    diag.clear();

    // Serialize the close message
    chan.serialize(com_stmt_close_packet{stmt.id()}, chan.reset_sequence_number());

    // Send it. No response is sent back
    return chan.async_write(std::move(handler));
}

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_CLOSE_STATEMENT_HPP_ */
