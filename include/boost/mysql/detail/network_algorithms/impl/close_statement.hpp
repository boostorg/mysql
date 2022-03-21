//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_CLOSE_STATEMENT_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_CLOSE_STATEMENT_HPP

#pragma once

#include <boost/mysql/detail/network_algorithms/close_statement.hpp>
#include <boost/mysql/detail/protocol/prepared_statement_messages.hpp>

template <class Stream>
void boost::mysql::detail::close_statement(
    channel<Stream>& chan,
    std::uint32_t statement_id,
    error_code& code,
    error_info&
)
{
    // Compose the close message
    com_stmt_close_packet packet {statement_id};

    // Serialize it
    serialize_message(packet, chan.current_capabilities(), chan.shared_buffer());

    // Send it. No response is sent back
    chan.reset_sequence_number();
    chan.write(boost::asio::buffer(chan.shared_buffer()), code);
}

template <class Stream, class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code)
)
boost::mysql::detail::async_close_statement(
    channel<Stream>& chan,
    std::uint32_t statement_id,
    CompletionToken&& token,
    error_info&
)
{
    // Compose the close message
    com_stmt_close_packet packet {statement_id};

    // Serialize it
    serialize_message(packet, chan.current_capabilities(), chan.shared_buffer());

    // Send it. No response is sent back
    chan.reset_sequence_number();
    return chan.async_write(
        boost::asio::buffer(chan.shared_buffer()),
        std::forward<CompletionToken>(token)
    );
}


#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_CLOSE_STATEMENT_HPP_ */
