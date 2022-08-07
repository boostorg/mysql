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
#include <boost/mysql/resultset.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/buffer.hpp>

namespace boost {
namespace mysql {
namespace detail {

inline bool process_read_message(
    boost::asio::const_buffer read_message,
    capabilities current_capabilities,
    resultset& resultset,
	row& output,
    error_code& err,
    error_info& info
)
{
    // Clear whatever was in the row before
    output.clear();

    // Deserialize the row
    bool row_read = deserialize_row(
        read_message,
        current_capabilities,
        resultset,
        output.values(),
        err,
        info
    );

    // Copy the strings into the row's buffer
    if (row_read)
        output.copy_strings();

    return row_read;
}

template<class Stream>
struct read_one_row_op : boost::asio::coroutine
{
    channel<Stream>& chan_;
    error_info& output_info_;
    resultset& resultset_;
    row& output_;

    read_one_row_op(
        channel<Stream>& chan,
        error_info& output_info,
        resultset& resultset,
		row& output
    ) noexcept :
        chan_(chan),
        output_info_(output_info),
        resultset_(resultset),
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
            result = process_read_message(
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
    resultset& resultset,
	row& output,
    error_code& err,
    error_info& info
)
{
    // If the resultset is already complete, we don't need to read anything
    if (resultset.complete())
    {
        output.clear();
        return false;
    }

    // Read a packet
    auto read_message = channel.read_one(resultset.sequence_number(), err);
    if (err)
        return false;

    return process_read_message(
        read_message,
        channel.current_capabilities(),
        resultset,
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
    resultset& resultset,
	row& output,
    error_info& output_info,
    CompletionToken&& token
)
{
    return boost::asio::async_compose<CompletionToken, void(error_code, bool)> (
        read_one_row_op<Stream>(
            channel,
            output_info,
            resultset,
			output
        ),
        token,
        channel
    );
}

#endif /* INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_READ_TEXT_ROW_IPP_ */
