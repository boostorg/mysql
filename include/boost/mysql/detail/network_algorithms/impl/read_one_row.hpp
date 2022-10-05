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
#include <boost/asio/post.hpp>
#include <boost/asio/buffer.hpp>
#include <vector>

namespace boost {
namespace mysql {
namespace detail {

template <class Stream>
inline bool process_one_row(
    boost::asio::const_buffer read_message,
    channel<Stream>& channel,
    resultset_base& result,
	row_view& output,
    error_code& err,
    error_info& info
)
{
    // Clear any previous field in the channel
    channel.shared_fields().clear();

    // Deserialize the row
    bool row_read = deserialize_row(
        read_message,
        channel.current_capabilities(),
        channel.buffer_first(),
        result,
        channel.shared_fields(),
        err,
        info
    );

    // Set the output variable
    if (row_read)
    {
        offsets_to_string_views(channel.shared_fields(), channel.buffer_first());
        output = row_view(channel.shared_fields().data(), channel.shared_fields().size());
    }
    else
    {
        output = row_view();
    }

    return row_read;
}

template<class Stream>
struct read_one_row_op : boost::asio::coroutine
{
    channel<Stream>& chan_;
    error_info& output_info_;
    resultset_base& resultset_;
    row& output_;

    read_one_row_op(
        channel<Stream>& chan,
        error_info& output_info,
        resultset_base& result,
		row& output
    ) noexcept :
        chan_(chan),
        output_info_(output_info),
        resultset_(result),
		output_(output)
    {
    }

    template<class Self>
    void operator()(
        Self& self,
        error_code err = {},
        boost::asio::const_buffer read_message = {}
    )
    {
        // Error checking
        if (err)
        {
            self.complete(err, false);
            return;
        }

        // Normal path
        bool result;
        BOOST_ASIO_CORO_REENTER(*this)
        {
            // If the resultset is already complete, we don't need to read anything
            if (resultset_.complete())
            {
                BOOST_ASIO_CORO_YIELD boost::asio::post(std::move(self));
                output_.clear();
                self.complete(error_code(), false);
                BOOST_ASIO_CORO_YIELD break;
            }

            // Read the message
            BOOST_ASIO_CORO_YIELD chan_.async_read_one(resultset_.sequence_number(), std::move(self));

            // Process it
            result = process_one_row(
                read_message,
                chan_.current_capabilities(),
                resultset_,
                output_,
                err,
                output_info_
            );
            self.complete(err, result);
        }
    }
};

} // detail
} // mysql
} // boost


template <class Stream>
bool boost::mysql::detail::read_one_row(
    channel<Stream>& channel,
    resultset_base& result,
	row_view& output,
    error_code& err,
    error_info& info
)
{
    // If the resultset is already complete, we don't need to read anything
    if (result.complete())
    {
        output = row_view();
        return false;
    }

    // Read a packet
    auto read_message = channel.read_one(result.sequence_number(), err);
    if (err)
        return false;

    return process_one_row(
        read_message,
        channel.current_capabilities(),
        result,
		output,
        err,
        info
    );
}

template <class Stream, class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code, bool)
)
boost::mysql::detail::async_read_one_row(
    channel<Stream>& channel,
    resultset_base& result,
	row_view& output,
    error_info& output_info,
    CompletionToken&& token
)
{
    return boost::asio::async_compose<CompletionToken, void(error_code, bool)> (
        read_one_row_op<Stream>(
            channel,
            output_info,
            result,
			output
        ),
        token,
        channel
    );
}

#endif /* INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_READ_TEXT_ROW_IPP_ */
