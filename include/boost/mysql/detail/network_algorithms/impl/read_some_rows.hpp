//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_READsome__ROW_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_READsome__ROW_HPP

#pragma once

#include <boost/asio/post.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/mysql/error.hpp>
#include <boost/mysql/row.hpp>
#include <boost/mysql/resultset.hpp>
#include <boost/mysql/detail/network_algorithms/read_some_rows.hpp>
#include <boost/mysql/detail/protocol/text_deserialization.hpp>
#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <cstddef>


namespace boost {
namespace mysql {
namespace detail {

inline bool deserialize_row(
    boost::asio::const_buffer row_message,
    capabilities current_capabilities,
    resultset& resultset,
	std::vector<value>& output,
    error_code& err,
    error_info& info
)
{
    // Message type: row, error or eof?
    std::uint8_t msg_type = 0;
    deserialization_context ctx (row_message, current_capabilities);
    err = make_error_code(deserialize(ctx, msg_type));
    if (err)
        return false;
    if (msg_type == eof_packet_header)
    {
        // end of resultset => this is a ok_packet, not a row
        ok_packet ok_pack;
        err = deserialize_message(ctx, ok_pack);
        if (err)
            return false;
        resultset.complete(ok_pack);
        output.clear();
        return false;
    }
    else if (msg_type == error_packet_header)
    {
        // An error occurred during the generation of the rows
        err = process_error_packet(ctx, info);
        return false;
    }
    else
    {
        // An actual row
        output.clear();
        ctx.rewind(1); // keep the 'message type' byte, as it is part of the actual message
        err = resultset.deserialize_row(ctx, output);
        if (err)
            return false;
        return true;
    }
}

template <class Stream>
inline void process_rows(
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
    channel.shared_values().clear();
    while (channel.num_read_messages())
    {
        // Get the row message
        auto message = channel.next_read_message(resultset.sequence_number(), err);
        if (err)
            return;

        // Deserialize the row. Values are stored in a vector owned by the channel
        bool row_read = deserialize_row(
            message,
            channel.current_capabilities(),
            resultset,
            channel.shared_values(),
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

    output = rows_view(
        channel.shared_values().data(),
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
            self.complete(err, false);
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
            BOOST_ASIO_CORO_YIELD chan_.async_read(1, std::move(self));

            // Process messages
            process_rows(chan_, resultset_, output_, err, output_info_);
            
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
    channel.read(1, err);
    if (err)
        return;

    // Process read messages
    process_rows(channel, resultset, output, err, info);
}

template <class Stream, class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code)
)
boost::mysql::detail::async_read_some_rows(
    channel<Stream>& channel,
    resultset& resultset,
	row& output,
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
