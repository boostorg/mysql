//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_IMPL_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_IMPL_HPP

#pragma once

#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/network_algorithms/execute_impl.hpp>
#include <boost/mysql/detail/network_algorithms/read_resultset_head.hpp>
#include <boost/mysql/detail/network_algorithms/read_some_rows_impl.hpp>
#include <boost/mysql/detail/protocol/deserialize_execution_messages.hpp>

#include <boost/asio/coroutine.hpp>

namespace boost {
namespace mysql {
namespace detail {

template <class Stream>
struct execute_impl_op : boost::asio::coroutine
{
    channel<Stream>& chan_;
    resultset_encoding enc_;
    execution_processor& output_;
    diagnostics& diag_;

    execute_impl_op(
        channel<Stream>& chan,
        resultset_encoding enc,
        execution_processor& output,
        diagnostics& diag
    ) noexcept
        : chan_(chan), enc_(enc), output_(output), diag_(diag)
    {
    }

    template <class Self>
    void operator()(Self& self, error_code err = {}, std::size_t = 0)
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
            output_.reset(enc_, chan_.meta_mode());

            // Send the execution request (already serialized at this point)
            BOOST_ASIO_CORO_YIELD chan_
                .async_write(chan_.shared_buffer(), output_.sequence_number(), std::move(self));

            while (!output_.is_complete())
            {
                if (output_.is_reading_head())
                {
                    BOOST_ASIO_CORO_YIELD async_read_resultset_head(chan_, output_, diag_, std::move(self));
                }
                else if (output_.is_reading_rows())
                {
                    BOOST_ASIO_CORO_YIELD
                    async_read_some_rows_impl(chan_, output_, output_ref(), diag_, std::move(self));
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
    execution_processor& output,
    error_code& err,
    diagnostics& diag
)
{
    // Setup
    diag.clear();
    output.reset(enc, channel.meta_mode());

    // Send the execution request (already serialized at this point)
    channel.write(channel.shared_buffer(), output.sequence_number(), err);
    if (err)
        return;

    while (!output.is_complete())
    {
        if (output.is_reading_head())
        {
            read_resultset_head(channel, output, err, diag);
            if (err)
                return;
        }
        else if (output.is_reading_rows())
        {
            read_some_rows_impl(channel, output, output_ref(), err, diag);
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
    execution_processor& output,
    diagnostics& diag,
    CompletionToken&& token
)
{
    return boost::asio::async_compose<CompletionToken, void(boost::mysql::error_code)>(
        execute_impl_op<Stream>(chan, enc, output, diag),
        token,
        chan
    );
}

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_QUERY_HPP_ */
