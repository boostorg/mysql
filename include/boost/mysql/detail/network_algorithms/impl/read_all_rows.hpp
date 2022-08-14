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
#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/error.hpp>
#include <boost/mysql/resultset.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/coroutine.hpp>
#include <cstddef>


namespace boost {
namespace mysql {
namespace detail {

inline void rebase_strings(
    const std::uint8_t* old_buffer_first,
    const std::uint8_t* new_buffer_first,
    std::vector<field_view>& fields
) noexcept
{
    auto diff = new_buffer_first - old_buffer_first;
    if (diff)
    {
        for (auto& f: fields)
        {
            const boost::string_view* str = f.if_string();
            if (str && !str->empty())
            {
                f = field_view(boost::string_view(
                    str->data() + diff,
                    str->size()
                ));
            }
        }
    }
}

template <class Stream>
inline void process_all_rows(
    channel<Stream>& channel,
    resultset& resultset,
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
        auto message = channel.next_read_message(resultset.sequence_number(), err);
        if (err)
            return;

        // Deserialize the row. Values are stored in a vector owned by the channel
        bool row_read = deserialize_row(
            message,
            channel.current_capabilities(),
            resultset,
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
                resultset.meta().size()
            );
            break;
        }
    }
}


template<class Stream>
struct read_all_rows_op : boost::asio::coroutine
{
    channel<Stream>& chan_;
    error_info& output_info_;
    resultset& resultset_;
    rows_view& output_;
    std::uint8_t* old_buffer_first_ {};

    read_all_rows_op(
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

            chan_.set_keep_messages(true);

            // Read at least one message
            while (!resultset_.complete())
            {
                // Keep track of the old buffer base
                old_buffer_first_ = chan_.buffer_first();

                // Actually read
                BOOST_ASIO_CORO_YIELD chan_.async_read_some(std::move(self));

                // Rebase strings if required
                rebase_strings(old_buffer_first_, chan_.buffer_first(), chan_.shared_fields());

                // Process messages
                process_all_rows(chan_, resultset_, output_, err, output_info_);
                if (err)
                {
                    self.complete(err);
                    BOOST_ASIO_CORO_YIELD break;
                }
            }

            // Done
            chan_.set_keep_messages(false);
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

    // TODO: should we use RAII here?
    channel.set_keep_messages(true);

    while (!resultset.complete())
    {
        const std::uint8_t* old_buffer_first = channel.buffer_first();

        // Read from the stream until there is at least one message
        channel.read_some(err);
        if (err)
            return;
        
        // Rebase strings if required
        rebase_strings(old_buffer_first, channel.buffer_first(), channel.shared_fields());

        // Process read messages
        process_all_rows(channel, resultset, output, err, info);
        if (err)
            return;
    }

    channel.set_keep_messages(false);
}

template <class Stream, class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code)
)
boost::mysql::detail::async_read_all_rows(
    channel<Stream>& channel,
    resultset& resultset,
	rows_view& output,
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
