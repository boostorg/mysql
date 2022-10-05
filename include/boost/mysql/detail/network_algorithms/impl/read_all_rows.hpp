//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_READ_ALL_ROWS_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_READ_ALL_ROWS_HPP

#pragma once

#include <boost/mysql/detail/network_algorithms/read_all_rows.hpp>
#include <boost/mysql/detail/protocol/deserialize_row.hpp>
#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/error.hpp>
#include <boost/mysql/resultset_base.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/coroutine.hpp>
#include <cstddef>


namespace boost {
namespace mysql {
namespace detail {

template <class Stream>
inline void process_all_rows(
    channel<Stream>& channel,
    resultset_base& result,
    rows_view& output,
    error_code& err,
    error_info& info
)
{
    // Process all read messages until they run out, an error happens
    // or an EOF is received
    while (channel.has_read_messages())
    {
        // Get the row message
        auto message = channel.next_read_message(result.sequence_number(), err);
        if (err)
            return;

        // Deserialize the row. Values are stored in a vector owned by the channel
        bool row_read = deserialize_row(
            message,
            channel.current_capabilities(),
            channel.buffer_first(),
            result,
            channel.shared_fields(),
            err,
            info
        );
        if (err)
            return;
        
        // If we received an EOF, we're done
        if (!row_read)
        {
            output = rows_view(
                channel.shared_fields().data(),
                channel.shared_fields().size(),
                result.meta().size()
            );
            offsets_to_string_views(channel.shared_fields(), channel.buffer_first());
            break;
        }
    }
}


template<class Stream>
struct read_all_rows_op : boost::asio::coroutine
{
    channel<Stream>& chan_;
    error_info& output_info_;
    resultset_base& resultset_;
    rows_view& output_;

    read_all_rows_op(
        channel<Stream>& chan,
        error_info& output_info,
        resultset_base& result,
		rows_view& output
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
            // If the resultset_base is already complete, we don't need to read anything
            if (resultset_.complete())
            {
                BOOST_ASIO_CORO_YIELD boost::asio::post(std::move(self));
                output_ = rows_view();
                self.complete(error_code());
                BOOST_ASIO_CORO_YIELD break;
            }

            // Read at least one message
            while (!resultset_.complete())
            {
                // Actually read
                BOOST_ASIO_CORO_YIELD chan_.async_read_some(std::move(self), true);

                // Process messages
                process_all_rows(chan_, resultset_, output_, err, output_info_);
                if (err)
                {
                    self.complete(err);
                    BOOST_ASIO_CORO_YIELD break;
                }
            }

            // Done
            self.complete(error_code());
        }
    }
};

} // detail
} // mysql
} // boost


template <class Stream>
void boost::mysql::detail::read_all_rows(
    channel<Stream>& channel,
    resultset_base& result,
	rows_view& output,
    error_code& err,
    error_info& info
)
{
    // If the resultset_base is already complete, we don't need to read anything
    if (result.complete())
    {
        output = rows_view();
        return;
    }

    while (!result.complete())
    {
        // Read from the stream until there is at least one message
        channel.read_some(err, true);
        if (err)
            return;
        
        // Process read messages
        process_all_rows(channel, result, output, err, info);
        if (err)
            return;
    }
}

template <class Stream, class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code)
)
boost::mysql::detail::async_read_all_rows(
    channel<Stream>& channel,
    resultset_base& result,
	rows_view& output,
    error_info& output_info,
    CompletionToken&& token
)
{
    return boost::asio::async_compose<CompletionToken, void(error_code)> (
        read_all_rows_op<Stream>(
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
