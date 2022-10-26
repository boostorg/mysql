//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_READ_SOME_ROWS_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_READ_SOME_ROWS_HPP

#pragma once

#include <boost/mysql/detail/network_algorithms/read_some_rows.hpp>
#include <boost/mysql/detail/protocol/deserialize_row.hpp>
#include <boost/mysql/error.hpp>
#include <boost/mysql/resultset_base.hpp>
#include <boost/mysql/row.hpp>

#include <boost/asio/buffer.hpp>
#include <boost/asio/post.hpp>

#include <cstddef>

namespace boost {
namespace mysql {
namespace detail {

template <class Stream>
inline rows_view process_some_rows(
    channel<Stream>& channel,
    resultset_base& result,
    error_code& err,
    error_info& info
)
{
    // Process all read messages until they run out, an error happens
    // or an EOF is received
    std::size_t num_rows = 0;
    channel.shared_fields().clear();
    while (channel.has_read_messages())
    {
        // Get the row message
        auto message = channel.next_read_message(result.sequence_number(), err);
        if (err)
            return rows_view();

        // Deserialize the row. Values are stored in a vector owned by the channel
        deserialize_row(
            message,
            channel.current_capabilities(),
            channel.buffer_first(),
            result,
            channel.shared_fields(),
            err,
            info
        );
        if (err)
            return rows_view();

        // There is no need to copy strings values anywhere; the returned values
        // will point into the channel's internal buffer

        // If we received an EOF, we're done
        if (result.complete())
            break;
        ++num_rows;
    }
    offsets_to_string_views(channel.shared_fields(), channel.buffer_first());

    return rows_view(
        channel.shared_fields().data(),
        num_rows * result.fields().size(),
        result.fields().size()
    );
}

template <class Stream>
struct read_some_rows_op : boost::asio::coroutine
{
    channel<Stream>& chan_;
    error_info& output_info_;
    resultset_base& resultset_;

    read_some_rows_op(
        channel<Stream>& chan,
        error_info& output_info,
        resultset_base& result
    ) noexcept
        : chan_(chan), output_info_(output_info), resultset_(result)
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
        rows_view output;
        BOOST_ASIO_CORO_REENTER(*this)
        {
            output_info_.clear();

            // If the resultset is already complete, we don't need to read anything
            if (resultset_.complete())
            {
                BOOST_ASIO_CORO_YIELD boost::asio::post(std::move(self));
                self.complete(error_code(), rows_view());
                BOOST_ASIO_CORO_YIELD break;
            }

            // Read at least one message
            BOOST_ASIO_CORO_YIELD chan_.async_read_some(std::move(self));

            // Process messages
            output = process_some_rows(chan_, resultset_, err, output_info_);

            self.complete(err, output);
        }
    }
};

template <class Stream>
struct read_some_rows_op_rows : boost::asio::coroutine
{
    channel<Stream>& chan_;
    error_info& output_info_;
    resultset_base& resultset_;
    rows& output_;

    read_some_rows_op_rows(
        channel<Stream>& chan,
        error_info& output_info,
        resultset_base& result,
        rows& output
    ) noexcept
        : chan_(chan), output_info_(output_info), resultset_(result), output_(output)
    {
    }

    template <class Self>
    void operator()(Self& self, error_code err = {}, rows_view rv = {})
    {
        // Error checking
        if (err)
        {
            self.complete(err);
            return;
        }

        // Normal path
        BOOST_ASIO_CORO_REENTER(*this)
        {
            // output_info already cleared by child ops
            output_.clear();

            BOOST_ASIO_CORO_YIELD
            async_read_some_rows(chan_, resultset_, output_info_, std::move(self));

            output_ = rv;
            self.complete(err);
        }
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

template <class Stream>
boost::mysql::rows_view boost::mysql::detail::read_some_rows(
    channel<Stream>& channel,
    resultset_base& result,
    error_code& err,
    error_info& info
)
{
    // If the resultset is already complete, we don't need to read anything
    if (result.complete())
    {
        return rows_view();
    }

    // Read from the stream until there is at least one message
    channel.read_some(err);
    if (err)
        return rows_view();

    // Process read messages
    return process_some_rows(channel, result, err, info);
}

template <class Stream>
void boost::mysql::detail::read_some_rows(
    channel<Stream>& channel,
    resultset_base& result,
    rows& output,
    error_code& err,
    error_info& info
)
{
    // TODO: this can be optimized
    output = read_some_rows(channel, result, err, info);
}

template <class Stream, class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code, boost::mysql::rows_view)
)
boost::mysql::detail::async_read_some_rows(
    channel<Stream>& channel,
    resultset_base& result,
    error_info& output_info,
    CompletionToken&& token
)
{
    return boost::asio::async_compose<CompletionToken, void(error_code, rows_view)>(
        read_some_rows_op<Stream>(channel, output_info, result),
        token,
        channel
    );
}

template <class Stream, class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::detail::async_read_some_rows(
    channel<Stream>& channel,
    resultset_base& result,
    rows& output,
    error_info& output_info,
    CompletionToken&& token
)
{
    return boost::asio::async_compose<CompletionToken, void(error_code)>(
        read_some_rows_op_rows<Stream>(channel, output_info, result, output),
        token,
        channel
    );
}

#endif /* INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_READ_TEXT_ROW_IPP_ */
