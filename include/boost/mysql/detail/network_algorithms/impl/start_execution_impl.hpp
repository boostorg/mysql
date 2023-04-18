//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_START_EXECUTION_IMPL_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_START_EXECUTION_IMPL_HPP

#pragma once

#include <boost/mysql/detail/network_algorithms/read_resultset_head.hpp>
#include <boost/mysql/detail/network_algorithms/start_execution_impl.hpp>

#include <boost/asio/coroutine.hpp>

namespace boost {
namespace mysql {
namespace detail {

template <class Stream>
struct start_execution_impl_op : boost::asio::coroutine
{
    channel<Stream>& chan_;
    resultset_encoding enc_;
    execution_processor& proc_;
    diagnostics& diag_;

    start_execution_impl_op(
        channel<Stream>& chan,
        resultset_encoding enc,
        execution_processor& proc,
        diagnostics& diag
    )
        : chan_(chan), enc_(enc), proc_(proc), diag_(diag)
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

        // Non-error path
        BOOST_ASIO_CORO_REENTER(*this)
        {
            // Setup
            diag_.clear();
            proc_.reset(enc_, chan_.meta_mode());

            // Send the execution request (already serialized at this point)
            BOOST_ASIO_CORO_YIELD chan_
                .async_write(chan_.shared_buffer(), proc_.sequence_number(), std::move(self));

            // Read the first resultset's head
            BOOST_ASIO_CORO_YIELD
            async_read_resultset_head(chan_, proc_, diag_, std::move(self));

            self.complete(error_code());
        }
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

template <class Stream>
void boost::mysql::detail::start_execution_impl(
    channel<Stream>& channel,
    resultset_encoding enc,
    execution_processor& proc,
    error_code& err,
    diagnostics& diag
)
{
    // Setup
    diag.clear();
    proc.reset(enc, channel.meta_mode());

    // Send the execution request (already serialized at this point)
    channel.write(channel.shared_buffer(), proc.sequence_number(), err);
    if (err)
        return;

    // Read the first resultset's head
    read_resultset_head(channel, proc, err, diag);
    if (err)
        return;
}

template <class Stream, BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::detail::async_start_execution_impl(
    channel<Stream>& channel,
    resultset_encoding enc,
    execution_processor& proc,
    diagnostics& diag,
    CompletionToken&& token
)
{
    return boost::asio::async_compose<CompletionToken, void(error_code)>(
        start_execution_impl_op<Stream>(channel, enc, proc, diag),
        token,
        channel
    );
}

#endif
