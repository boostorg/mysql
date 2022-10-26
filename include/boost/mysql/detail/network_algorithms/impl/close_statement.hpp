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

namespace boost {
namespace mysql {
namespace detail {

template <class Stream>
struct close_statement_op : boost::asio::coroutine
{
    channel<Stream>& chan_;
    statement_base& stmt_;
    error_info& output_info_;

    close_statement_op(
        channel<Stream>& chan,
        statement_base& stmt,
        error_info& output_info
    ) noexcept
        : chan_(chan), stmt_(stmt), output_info_(output_info)
    {
    }

    template <class Self>
    void operator()(Self& self, error_code err = {})
    {
        // Error checking
        if (err)
        {
            self.complete(err);
            return;
        }

        // Regular coroutine body; if there has been an error, we don't get here
        BOOST_ASIO_CORO_REENTER(*this)
        {
            output_info_.clear();

            // Serialize the close message
            serialize_message(
                com_stmt_close_packet{stmt_.id()},
                chan_.current_capabilities(),
                chan_.shared_buffer()
            );

            // Write message (already serialized at this point)
            BOOST_ASIO_CORO_YIELD chan_
                .async_write(chan_.shared_buffer(), chan_.reset_sequence_number(), std::move(self));

            // Mark the statement as invalid
            stmt_.reset();

            // Complete
            self.complete(error_code());
        }
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

template <class Stream>
void boost::mysql::detail::
    close_statement(channel<Stream>& chan, statement_base& stmt, error_code& code, error_info&)
{
    // Serialize the close message
    serialize_message(
        com_stmt_close_packet{stmt.id()},
        chan.current_capabilities(),
        chan.shared_buffer()
    );

    // Send it. No response is sent back
    chan.write(boost::asio::buffer(chan.shared_buffer()), chan.reset_sequence_number(), code);

    // Mark the statement as invalid
    stmt.reset();
}

template <class Stream, class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::detail::async_close_statement(
    channel<Stream>& chan,
    statement_base& stmt,
    CompletionToken&& token,
    error_info& output_info
)
{
    return boost::asio::async_compose<CompletionToken, void(error_code)>(
        close_statement_op<Stream>(chan, stmt, output_info),
        token,
        chan
    );
}

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_CLOSE_STATEMENT_HPP_ */
