//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_READ_SOME_ROWS_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_READ_SOME_ROWS_HPP

#pragma once
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/row.hpp>

#include <boost/mysql/detail/auxiliar/row_impl.hpp>
#include <boost/mysql/detail/network_algorithms/helpers.hpp>
#include <boost/mysql/detail/network_algorithms/read_some_rows.hpp>
#include <boost/mysql/detail/protocol/deserialize_row.hpp>

#include <boost/asio/buffer.hpp>
#include <boost/asio/post.hpp>

#include <cstddef>

namespace boost {
namespace mysql {
namespace detail {

template <class Stream>
struct read_some_rows_op : boost::asio::coroutine
{
    channel<Stream>& chan_;
    diagnostics& diag_;
    execution_state_impl& st_;

    read_some_rows_op(channel<Stream>& chan, diagnostics& diag, execution_state_impl& st) noexcept
        : chan_(chan), diag_(diag), st_(st)
    {
    }

    template <class Self>
    void operator()(Self& self, error_code err = {})
    {
        // Error checking
        if (err)
        {
            self.complete(err, rows_view());
            return;
        }

        // Normal path
        BOOST_ASIO_CORO_REENTER(*this)
        {
            diag_.clear();

            // If the op is already complete, we don't need to read anything
            if (st_.complete())
            {
                BOOST_ASIO_CORO_YIELD boost::asio::post(std::move(self));
                self.complete(error_code(), rows_view());
                BOOST_ASIO_CORO_YIELD break;
            }

            // Read at least one message
            BOOST_ASIO_CORO_YIELD chan_.async_read_some(std::move(self));

            // Process messages
            process_available_rows(chan_, st_, err, diag_);
            if (err)
            {
                self.complete(err, rows_view());
                BOOST_ASIO_CORO_YIELD break;
            }

            self.complete(error_code(), st_.get_external_rows());
        }
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

template <class Stream>
boost::mysql::rows_view boost::mysql::detail::read_some_rows(
    channel<Stream>& channel,
    execution_state& st,
    error_code& err,
    diagnostics& diag
)
{
    auto& impl = execution_state_access::get_impl(st);

    // If the op is already complete, we don't need to read anything
    if (impl.complete())
    {
        return rows_view();
    }

    // Read from the stream until there is at least one message
    channel.read_some(err);
    if (err)
        return rows_view();

    // Process read messages
    process_available_rows(channel, impl, err, diag);
    if (err)
        return rows_view();

    return impl.get_external_rows();
}

template <
    class Stream,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::rows_view))
        CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code, boost::mysql::rows_view))
boost::mysql::detail::async_read_some_rows(
    channel<Stream>& channel,
    execution_state& st,
    diagnostics& diag,
    CompletionToken&& token
)
{
    return boost::asio::async_compose<CompletionToken, void(error_code, rows_view)>(
        read_some_rows_op<Stream>(channel, diag, execution_state_access::get_impl(st)),
        token,
        channel
    );
}

#endif /* INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_READ_TEXT_ROW_IPP_ */
