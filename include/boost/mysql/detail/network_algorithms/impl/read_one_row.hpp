//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_READ_ONE_ROW_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_READ_ONE_ROW_HPP

#pragma once

#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/row_view.hpp>

#include <boost/mysql/detail/network_algorithms/read_one_row.hpp>
#include <boost/mysql/detail/protocol/deserialize_row.hpp>

#include <boost/asio/buffer.hpp>
#include <boost/asio/post.hpp>

#include <vector>

namespace boost {
namespace mysql {
namespace detail {

template <class Stream>
inline row_view process_one_row(
    channel<Stream>& channel,
    boost::asio::const_buffer read_message,
    execution_state& st,
    error_code& err,
    error_info& info
)
{
    // Clear any previous field in the channel
    channel.shared_fields().clear();

    // Deserialize the row
    deserialize_row(
        read_message,
        channel.current_capabilities(),
        channel.buffer_first(),
        st,
        channel.shared_fields(),
        err,
        info
    );

    if (!err && !st.complete())
    {
        offsets_to_string_views(channel.shared_fields(), channel.buffer_first());
        return row_view(channel.shared_fields().data(), channel.shared_fields().size());
    }
    else
    {
        return row_view();
    }
}

template <class Stream>
struct read_one_row_op : boost::asio::coroutine
{
    channel<Stream>& chan_;
    error_info& output_info_;
    execution_state& st_;

    read_one_row_op(channel<Stream>& chan, error_info& output_info, execution_state& st) noexcept
        : chan_(chan), output_info_(output_info), st_(st)
    {
    }

    template <class Self>
    void operator()(Self& self, error_code err = {}, boost::asio::const_buffer read_message = {})
    {
        // Error checking
        if (err)
        {
            self.complete(err, row_view());
            return;
        }

        // Normal path
        row_view result;
        BOOST_ASIO_CORO_REENTER(*this)
        {
            output_info_.clear();

            // If the resultset is already complete, we don't need to read anything
            if (st_.complete())
            {
                BOOST_ASIO_CORO_YIELD boost::asio::post(std::move(self));
                self.complete(error_code(), row_view());
                BOOST_ASIO_CORO_YIELD break;
            }

            // Read the message
            BOOST_ASIO_CORO_YIELD chan_.async_read_one(st_.sequence_number(), std::move(self));

            // Process it
            result = process_one_row(chan_, read_message, st_, err, output_info_);
            self.complete(err, result);
        }
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

template <class Stream>
boost::mysql::row_view boost::mysql::detail::read_one_row(
    channel<Stream>& channel,
    execution_state& result,
    error_code& err,
    error_info& info
)
{
    // If the resultset is already complete, we don't need to read anything
    if (result.complete())
    {
        return row_view();
    }

    // Read a packet
    auto read_message = channel.read_one(result.sequence_number(), err);
    if (err)
        return row_view();

    return process_one_row(channel, read_message, result, err, info);
}

template <
    class Stream,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::row_view))
        CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code, boost::mysql::row_view)
)
boost::mysql::detail::async_read_one_row(
    channel<Stream>& channel,
    execution_state& st,
    error_info& output_info,
    CompletionToken&& token
)
{
    return boost::asio::async_compose<CompletionToken, void(error_code, row_view)>(
        read_one_row_op<Stream>(channel, output_info, st),
        token,
        channel
    );
}

#endif /* INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_READ_TEXT_ROW_IPP_ */
