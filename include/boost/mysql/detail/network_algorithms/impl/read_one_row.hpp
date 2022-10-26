//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_READ_ONE_ROW_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_READ_ONE_ROW_HPP

#pragma once

#include <boost/mysql/detail/network_algorithms/read_one_row.hpp>
#include <boost/mysql/detail/protocol/deserialize_row.hpp>
#include <boost/mysql/resultset_base.hpp>
#include <boost/mysql/row_view.hpp>

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
    resultset_base& result,
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
        result,
        channel.shared_fields(),
        err,
        info
    );

    if (!err && !result.complete())
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
    resultset_base& resultset_;

    read_one_row_op(channel<Stream>& chan, error_info& output_info, resultset_base& result) noexcept
        : chan_(chan), output_info_(output_info), resultset_(result)
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
            if (resultset_.complete())
            {
                BOOST_ASIO_CORO_YIELD boost::asio::post(std::move(self));
                self.complete(error_code(), row_view());
                BOOST_ASIO_CORO_YIELD break;
            }

            // Read the message
            BOOST_ASIO_CORO_YIELD chan_.async_read_one(
                resultset_.sequence_number(),
                std::move(self)
            );

            // Process it
            result = process_one_row(chan_, read_message, resultset_, err, output_info_);
            self.complete(err, result);
        }
    }
};

template <class Stream>
struct read_one_row_op_row : boost::asio::coroutine
{
    channel<Stream>& chan_;
    error_info& output_info_;
    resultset_base& resultset_;
    row& output_;

    read_one_row_op_row(
        channel<Stream>& chan,
        error_info& output_info,
        resultset_base& result,
        row& output
    ) noexcept
        : chan_(chan), output_info_(output_info), resultset_(result), output_(output)
    {
    }

    template <class Self>
    void operator()(Self& self, error_code err = {}, row_view rv = {})
    {
        // Error checking
        if (err)
        {
            self.complete(err, false);
            return;
        }

        // Normal path
        BOOST_ASIO_CORO_REENTER(*this)
        {
            // error_info already cleared by child ops
            output_.clear();

            BOOST_ASIO_CORO_YIELD
            async_read_one_row(chan_, resultset_, output_info_, std::move(self));

            output_ = rv;
            self.complete(error_code(), !rv.empty());
        }
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

template <class Stream>
boost::mysql::row_view boost::mysql::detail::read_one_row(
    channel<Stream>& channel,
    resultset_base& result,
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

template <class Stream>
bool boost::mysql::detail::read_one_row(
    channel<Stream>& channel,
    resultset_base& result,
    row& output,
    error_code& err,
    error_info& info
)
{
    // TODO: this can be optimized
    output = read_one_row(channel, result, err, info);
    return !output.empty();
}

template <class Stream, class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code, boost::mysql::row_view)
)
boost::mysql::detail::async_read_one_row(
    channel<Stream>& channel,
    resultset_base& result,
    error_info& output_info,
    CompletionToken&& token
)
{
    return boost::asio::async_compose<CompletionToken, void(error_code, row_view)>(
        read_one_row_op<Stream>(channel, output_info, result),
        token,
        channel
    );
}

template <class Stream, class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code, bool))
boost::mysql::detail::async_read_one_row(
    channel<Stream>& channel,
    resultset_base& result,
    row& output,
    error_info& output_info,
    CompletionToken&& token
)
{
    return boost::asio::async_compose<CompletionToken, void(error_code, bool)>(
        read_one_row_op_row<Stream>(channel, output_info, result, output),
        token,
        channel
    );
}

#endif /* INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_READ_TEXT_ROW_IPP_ */
