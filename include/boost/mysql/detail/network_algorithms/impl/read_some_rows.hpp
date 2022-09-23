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
#include <boost/mysql/row.hpp>
#include <boost/mysql/resultset.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/buffer.hpp>
#include <cstddef>


namespace boost {
namespace mysql {
namespace detail {

template <class Stream>
inline void process_some_rows(
    channel<Stream>& channel,
    resultset& resultset,
    rows_view& output,
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
        auto message = channel.next_read_message(resultset.sequence_number(), err);
        if (err)
            return;

        // Deserialize the row. Values are stored in a vector owned by the channel
        bool row_read = deserialize_row(
            message,
            channel.current_capabilities(),
            channel.buffer_first(),
            resultset,
            channel.shared_fields(),
            err,
            info
        );
        if (err)
            return;
        
        // There is no need to copy strings values anywhere; the returned values
        // will point into the channel's internal buffer

        // If we received an EOF, we're done
        if (!row_read)
            break;
        ++num_rows;
    }
    offsets_to_string_views(channel.shared_fields(), channel.buffer_first());

    output = rows_view(
        channel.shared_fields().data(),
        num_rows * resultset.fields().size(),
        resultset.fields().size()
    );
}


template<class Stream>
struct read_some_rows_op : boost::asio::coroutine
{
    channel<Stream>& chan_;
    error_info& output_info_;
    resultset& resultset_;
    rows_view& output_;

    read_some_rows_op(
        channel<Stream>& chan,
        error_info& output_info,
        resultset& resultset,
		rows_view& output
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
        error_code err = {}
    )
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
            // If the resultset is already complete, we don't need to read anything
            if (resultset_.complete())
            {
                BOOST_ASIO_CORO_YIELD boost::asio::post(std::move(self));
                output_ = rows_view();
                self.complete(error_code());
                BOOST_ASIO_CORO_YIELD break;
            }

            // Read at least one message
            BOOST_ASIO_CORO_YIELD chan_.async_read_some(std::move(self));

            // Process messages
            process_some_rows(chan_, resultset_, output_, err, output_info_);
            
            self.complete(err);
        }
    }
};

} // detail
} // mysql
} // boost


template <class Stream>
void boost::mysql::detail::read_some_rows(
    channel<Stream>& channel,
    resultset& resultset,
	rows_view& output,
    error_code& err,
    error_info& info
)
{
    // If the resultset is already complete, we don't need to read anything
    if (resultset.complete())
    {
        output = rows_view();
        return;
    }

    // Read from the stream until there is at least one message
    channel.read_some(err);
    if (err)
        return;

    // Process read messages
    process_some_rows(channel, resultset, output, err, info);
}

template <class Stream, class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code)
)
boost::mysql::detail::async_read_some_rows(
    channel<Stream>& channel,
    resultset& resultset,
	rows_view& output,
    error_info& output_info,
    CompletionToken&& token
)
{
    return boost::asio::async_compose<CompletionToken, void(error_code)> (
        read_some_rows_op<Stream>(
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
