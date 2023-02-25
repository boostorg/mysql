//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_START_EXECUTION_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_START_EXECUTION_HPP

#pragma once

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/execution_state.hpp>

#include <boost/mysql/detail/auxiliar/bytestring.hpp>
#include <boost/mysql/detail/channel/channel.hpp>
#include <boost/mysql/detail/network_algorithms/read_resultset_head.hpp>
#include <boost/mysql/detail/network_algorithms/start_execution.hpp>
#include <boost/mysql/detail/protocol/capabilities.hpp>
#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/execution_state_impl.hpp>
#include <boost/mysql/detail/protocol/process_error_packet.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>
#include <boost/mysql/detail/protocol/serialization.hpp>

#include <boost/asio/buffer.hpp>
#include <boost/asio/coroutine.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

namespace boost {
namespace mysql {
namespace detail {

inline void start_execution_setup(
    channel_base& chan,
    bool append_mode,
    const execution_request& req,
    execution_state_impl& st,
    diagnostics& diag
)
{
    diag.clear();
    st.reset(req.encoding(), append_mode);
    req.serialize(chan.current_capabilities(), chan.shared_buffer());
}

template <class Stream>
struct start_execution_op : boost::asio::coroutine
{
    channel<Stream>& chan_;
    bool append_mode_;
    error_code fast_fail_;
    std::unique_ptr<execution_request> req_;  // TODO: this doesn't follow Asio's allocation model
    execution_state_impl& st_;
    diagnostics& diag_;

    start_execution_op(
        channel<Stream>& chan,
        bool append_mode,
        error_code fast_fail,
        std::unique_ptr<execution_request> req,
        execution_state_impl& st,
        diagnostics& diag
    )
        : chan_(chan),
          append_mode_(append_mode),
          fast_fail_(fast_fail),
          req_(std::move(req)),
          st_(st),
          diag_(diag)
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
            // If we got passed a fast-fail code (e.g. a statement number-of-params check failed), fail
            if (fast_fail_)
            {
                BOOST_ASIO_CORO_YIELD boost::asio::post(std::move(self));
                self.complete(fast_fail_);
                BOOST_ASIO_CORO_YIELD break;
            }

            // Setup & serialize
            start_execution_setup(chan_, append_mode_, *req_, st_, diag_);
            req_.reset();

            // Send it
            BOOST_ASIO_CORO_YIELD chan_
                .async_write(chan_.shared_buffer(), st_.sequence_number(), std::move(self));

            // Read the first resultset's head
            BOOST_ASIO_CORO_YIELD
            async_read_resultset_head(chan_, st_, diag_, std::move(self));
        }
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

template <class Stream>
void boost::mysql::detail::start_execution(
    channel<Stream>& channel,
    error_code fast_fail,
    bool append_mode,
    const execution_request& req,
    execution_state_impl& st,
    error_code& err,
    diagnostics& diag
)
{
    // If we got passed a fast-fail code (e.g. a statement number-of-params check failed), fail
    if (fast_fail)
    {
        err = fast_fail;
        return;
    }

    // Setup & serialize
    start_execution_setup(channel, append_mode, req, st, diag);

    // Send it
    channel.write(channel.shared_buffer(), st.sequence_number(), err);
    if (err)
        return;

    // Read the first resultset's head
    err = read_resultset_head(channel, st, err, diag);
    if (err)
        return;
}

template <class Stream, BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::detail::async_start_execution(
    channel<Stream>& channel,
    error_code fast_fail,
    bool append_mode,
    std::unique_ptr<execution_request> req,
    execution_state_impl& st,
    diagnostics& diag,
    CompletionToken&& token
)
{
    return boost::asio::async_compose<CompletionToken, void(error_code)>(
        start_execution_op<Stream>(channel, fast_fail, append_mode, std::move(req), st, diag),
        token,
        channel
    );
}

#endif
