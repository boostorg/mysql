//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_IMPL_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_IMPL_HPP

#pragma once

#include <boost/mysql/detail/network_algorithms/execute_impl.hpp>
#include <boost/mysql/detail/network_algorithms/helpers.hpp>
#include <boost/mysql/detail/network_algorithms/read_resultset_head.hpp>
#include <boost/mysql/detail/protocol/execution_processor.hpp>

#include <boost/asio/coroutine.hpp>

namespace boost {
namespace mysql {
namespace detail {

inline error_code process_available_rows(channel_base& channel, results_base& result, diagnostics& diag)
{
    // Process all read messages until they run out, an error happens
    // or an EOF is received
    result.on_row_batch_start();
    while (channel.has_read_messages() && result.should_read_rows())
    {
        auto err = process_row_message(channel, result, diag);
        if (err)
            return err;
    }
    result.on_row_batch_finish();
    return error_code();
}

template <class Stream>
struct execute_impl_op : boost::asio::coroutine
{
    channel<Stream>& chan_;
    resultset_encoding enc_;
    results_base& result_;
    diagnostics& diag_;

    execute_impl_op(
        channel<Stream>& chan,
        resultset_encoding enc,
        results_base& result,
        diagnostics& diag
    ) noexcept
        : chan_(chan), enc_(enc), result_(result), diag_(diag)
    {
    }

    template <class Self>
    void operator()(Self& self, error_code err = {})
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
            // Setup
            diag_.clear();
            result_.reset(enc_, chan_.meta_mode());

            // Send the execution request (already serialized at this point)
            BOOST_ASIO_CORO_YIELD chan_
                .async_write(chan_.shared_buffer(), result_.sequence_number(), std::move(self));

            while (!result_.complete())
            {
                BOOST_ASIO_CORO_YIELD async_read_resultset_head(chan_, result_, diag_, std::move(self));

                while (result_.should_read_rows())
                {
                    // Ensure we have messages to be read
                    if (!chan_.has_read_messages())
                    {
                        BOOST_ASIO_CORO_YIELD chan_.async_read_some(std::move(self));
                    }

                    // Process read messages
                    err = process_available_rows(chan_, result_, diag_);
                    if (err)
                    {
                        self.complete(err);
                        BOOST_ASIO_CORO_YIELD break;
                    }
                }
            }

            self.complete(error_code());
        }
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

template <class Stream>
void boost::mysql::detail::execute_impl(
    channel<Stream>& channel,
    resultset_encoding enc,
    results_base& result,
    error_code& err,
    diagnostics& diag
)
{
    // Setup
    diag.clear();
    result.reset(enc, channel.meta_mode());

    // Send the execution request (already serialized at this point)
    channel.write(channel.shared_buffer(), result.sequence_number(), err);
    if (err)
        return;

    while (!result.complete())
    {
        if (result.should_read_head())
        {
            read_resultset_head(channel, result, err, diag);
            if (err)
                return;
        }

        while (result.should_read_rows())
        {
            // Ensure we have messages to be read
            if (!channel.has_read_messages())
            {
                channel.read_some(err);
                if (err)
                    return;
            }

            // Process read messages
            err = process_available_rows(channel, result, diag);
            if (err)
                return;
        }
    }
}

template <class Stream, BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::detail::async_execute_impl(
    channel<Stream>& chan,
    resultset_encoding enc,
    results_base& result,
    diagnostics& diag,
    CompletionToken&& token
)
{
    return boost::asio::async_compose<CompletionToken, void(boost::mysql::error_code)>(
        execute_impl_op<Stream>(chan, enc, result, diag),
        token,
        chan
    );
}

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_QUERY_HPP_ */
