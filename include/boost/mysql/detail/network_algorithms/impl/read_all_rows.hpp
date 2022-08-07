//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_READsomeppp__ROW_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_READsomeppp__ROW_HPP

#pragma once

#include <boost/mysql/detail/network_algorithms/read_all_rows.hpp>
#include <boost/mysql/error.hpp>
#include <boost/mysql/row.hpp>
#include <boost/mysql/resultset.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/coroutine.hpp>
#include <cstddef>


namespace boost {
namespace mysql {
namespace detail {

template <class Stream>
inline void process_rows(
    channel<Stream>& channel,
    resultset& resultset,
    rows& output,
    error_code& err,
    error_info& info
)
{
    // The number of values we already have, required to copy strings
    std::size_t old_value_size = output.values().size();
    
    // Process all read messages until they run out, an error happens
    // or an EOF is received
    std::size_t num_rows = 0;
    while (channel.num_read_messages())
    {
        // Get the row message
        auto message = channel.next_read_message(resultset.sequence_number(), err);
        if (err)
            return;

        // Deserialize the row. Values are stored in a vector owned by rows object
        bool row_read = deserialize_row(
            message,
            channel.current_capabilities(),
            resultset,
            output.values(),
            err,
            info
        );
        if (err)
            return;
        
        // If we received an EOF, we're done
        if (!row_read)
            break;
        ++num_rows;
    }

    // Copy strings
    output.copy_strings(old_value_size);
}


template<class Stream>
struct read_all_rows_op : boost::asio::coroutine
{
    channel<Stream>& chan_;
    error_info& output_info_;
    resultset& resultset_;
    rows& output_;

    read_all_rows_op(
        channel<Stream>& chan,
        error_info& output_info,
        resultset& resultset,
		rows& output
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
                output_.clear();
                self.complete(error_code());
                BOOST_ASIO_CORO_YIELD break;
            }

            // Read at least one message
            while (!resultset_.complete())
            {
                BOOST_ASIO_CORO_YIELD chan_.async_read(1, std::move(self));

                // Process messages
                process_rows(chan_, resultset_, output_, err, output_info_);

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
    resultset& resultset,
	rows& output,
    error_code& err,
    error_info& info
)
{
    // If the resultset is already complete, we don't need to read anything
    if (resultset.complete())
    {
        output.clear();
        return;
    }

    while (!resultset.complete())
    {
        // Read from the stream until there is at least one message
        channel.read(1, err);
        if (err)
            return;
        
        // Process read messages
        process_rows(channel, resultset, output, err, info);
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
    resultset& resultset,
	rows& output,
    error_info& output_info,
    CompletionToken&& token
)
{
    return boost::asio::async_compose<CompletionToken, void(error_code)> (
        read_all_rows_op<Stream>(
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
