//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_START_EXECUTION_IMPL_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_START_EXECUTION_IMPL_HPP

#pragma once

#include <boost/mysql/detail/channel/channel.hpp>
#include <boost/mysql/detail/network_algorithms/read_resultset_head.hpp>
#include <boost/mysql/detail/network_algorithms/start_execution_impl.hpp>

#include <boost/asio/coroutine.hpp>
#include <boost/asio/post.hpp>

namespace boost {
namespace mysql {
namespace detail {

struct start_execution_impl_op : boost::asio::coroutine
{
    channel& chan_;
    error_code fast_fail_;
    resultset_encoding enc_;
    execution_processor& proc_;
    diagnostics& diag_;

    start_execution_impl_op(
        channel& chan,
        error_code fast_fail,
        resultset_encoding enc,
        execution_processor& proc,
        diagnostics& diag
    )
        : chan_(chan), fast_fail_(fast_fail), enc_(enc), proc_(proc), diag_(diag)
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
            diag_.clear();

            // Fast fail checking
            if (fast_fail_)
            {
                BOOST_ASIO_CORO_YIELD boost::asio::post(chan_.get_executor(), std::move(self));
                self.complete(fast_fail_);
                BOOST_ASIO_CORO_YIELD break;
            }

            // Setup
            proc_.reset(enc_, chan_.meta_mode());

            // Send the execution request (already serialized at this point)
            BOOST_ASIO_CORO_YIELD chan_
                .async_write(chan_.shared_buffer(), proc_.sequence_number(), std::move(self));

            // Read the first resultset's head
            BOOST_ASIO_CORO_YIELD
            async_read_resultset_head_impl(chan_, proc_, diag_, std::move(self));

            self.complete(error_code());
        }
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

void boost::mysql::detail::start_execution_impl(
    channel& channel,
    error_code fast_fail,
    resultset_encoding enc,
    execution_processor& proc,
    error_code& err,
    diagnostics& diag
)
{
    diag.clear();

    // Fast fail checking
    if (fast_fail)
    {
        err = fast_fail;
        return;
    }

    // Setup
    proc.reset(enc, channel.meta_mode());

    // Send the execution request (already serialized at this point)
    channel.write(channel.shared_buffer(), proc.sequence_number(), err);
    if (err)
        return;

    // Read the first resultset's head
    read_resultset_head_impl(channel, proc, err, diag);
    if (err)
        return;
}

void boost::mysql::detail::async_start_execution_impl(
    channel& channel,
    error_code fast_fail,
    resultset_encoding enc,
    execution_processor& proc,
    diagnostics& diag,
    asio::any_completion_handler<void(error_code)> handler
)
{
    return boost::asio::async_compose<asio::any_completion_handler<void(error_code)>, void(error_code)>(
        start_execution_impl_op(channel, fast_fail, enc, proc, diag),
        handler,
        channel
    );
}

#endif
