//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_HPP

#pragma once

#include <boost/mysql/detail/network_algorithms/execute.hpp>
#include <boost/mysql/detail/network_algorithms/helpers.hpp>
#include <boost/mysql/detail/network_algorithms/read_resultset_head.hpp>
#include <boost/mysql/detail/network_algorithms/start_execution.hpp>
#include <boost/mysql/detail/protocol/deserialize_row.hpp>
#include <boost/mysql/detail/protocol/execution_state_impl.hpp>

#include <boost/asio/coroutine.hpp>

namespace boost {
namespace mysql {
namespace detail {

template <class Stream>
struct execute_op : boost::asio::coroutine
{
    channel<Stream>& chan_;
    resultset_encoding enc_;
    execution_state_impl& st_;
    diagnostics& diag_;

    execute_op(
        channel<Stream>& chan,
        resultset_encoding enc,
        execution_state_impl& st,
        diagnostics& diag
    ) noexcept
        : chan_(chan), enc_(enc), st_(st), diag_(diag)
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
            st_.reset(enc_, nullptr);  // rows owned by st

            // Send the execution request (already serialized at this point)
            BOOST_ASIO_CORO_YIELD chan_
                .async_write(chan_.shared_buffer(), st_.sequence_number(), std::move(self));

            while (!st_.complete())
            {
                if (st_.should_read_head())
                {
                    BOOST_ASIO_CORO_YIELD async_read_resultset_head(chan_, st_, diag_, std::move(self));
                }

                while (st_.should_read_rows())
                {
                    // Ensure we have messages to be read
                    if (!chan_.has_read_messages())
                    {
                        BOOST_ASIO_CORO_YIELD chan_.async_read_some(std::move(self));
                    }

                    // Process read messages
                    process_available_rows(chan_, st_, err, diag_);
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
void boost::mysql::detail::execute(
    channel<Stream>& channel,
    resultset_encoding enc,
    results& result,
    error_code& err,
    diagnostics& diag
)
{
    // Setup
    diag.clear();
    auto& st = results_access::get_impl(result);
    st.reset(enc, nullptr);

    // Send the execution request (already serialized at this point)
    channel.write(channel.shared_buffer(), st.sequence_number(), err);
    if (err)
        return;

    while (!st.complete())
    {
        if (st.should_read_head())
        {
            read_resultset_head(channel, st, err, diag);
            if (err)
                return;
        }

        while (st.should_read_rows())
        {
            // Ensure we have messages to be read
            if (!channel.has_read_messages())
            {
                channel.read_some(err);
                if (err)
                    return;
            }

            // Process read messages
            process_available_rows(channel, st, err, diag);
            if (err)
                return;
        }
    }
}

template <class Stream, BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::detail::async_execute(
    channel<Stream>& chan,
    resultset_encoding enc,
    results& result,
    diagnostics& diag,
    CompletionToken&& token
)
{
    return boost::asio::async_compose<CompletionToken, void(boost::mysql::error_code)>(
        execute_op<Stream>(chan, enc, results_access::get_impl(result), diag),
        token,
        chan
    );
}

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_QUERY_HPP_ */
