//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_CLOSE_STATEMENT_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_CLOSE_STATEMENT_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/statement.hpp>

#include <boost/mysql/detail/channel/channel.hpp>
#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/protocol/prepared_statement_messages.hpp>

#include <boost/asio/async_result.hpp>

namespace boost {
namespace mysql {
namespace detail {

inline void close_statement_impl(channel& chan, const statement& stmt, error_code& err, diagnostics& diag)
{
    err.clear();
    diag.clear();

    // Serialize the close message
    chan.serialize(com_stmt_close_packet{stmt.id()}, chan.reset_sequence_number());

    // Send it. No response is sent back
    chan.write(err);
}

template <class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
async_close_statement_impl(channel& chan, const statement& stmt, diagnostics& diag, CompletionToken&& token)
{
    // We can do this here because we know no deferred tokens reach this function (thanks to erasing)
    diag.clear();

    // Serialize the close message
    chan.serialize(com_stmt_close_packet{stmt.id()}, chan.reset_sequence_number());

    // Send it. No response is sent back
    return chan.async_write(std::forward<CompletionToken>(token));
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_CLOSE_STATEMENT_HPP_ */
