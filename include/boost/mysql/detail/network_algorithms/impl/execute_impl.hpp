//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_IMPL_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_IMPL_HPP

#pragma once

#include <boost/mysql/detail/channel/channel.hpp>
#include <boost/mysql/detail/network_algorithms/execute_impl.hpp>
#include <boost/mysql/detail/network_algorithms/read_resultset_head.hpp>
#include <boost/mysql/detail/network_algorithms/read_some_rows_impl.hpp>
#include <boost/mysql/detail/network_algorithms/start_execution_impl.hpp>
#include <boost/mysql/detail/protocol/deserialize_execution_messages.hpp>

#include <boost/asio/coroutine.hpp>
#include <boost/asio/post.hpp>

namespace boost {
namespace mysql {
namespace detail {

struct execute_impl_op : boost::asio::coroutine
{
    channel& chan_;
    any_execution_request req_;
    execution_processor& output_;
    diagnostics& diag_;

    execute_impl_op(
        channel& chan,
        const any_execution_request& req,
        execution_processor& output,
        diagnostics& diag
    ) noexcept
        : chan_(chan), req_(req), output_(output), diag_(diag)
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
            // Send request and read the first response
            BOOST_ASIO_CORO_YIELD async_start_execution_impl(chan_, req_, output_, diag_, std::move(self));

            // Read anything else
            while (!output_.is_complete())
            {
                if (output_.is_reading_head())
                {
                    BOOST_ASIO_CORO_YIELD
                    async_read_resultset_head_impl(chan_, output_, diag_, std::move(self));
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

void boost::mysql::detail::execute_impl(
    channel& channel,
    const any_execution_request& req,
    execution_processor& output,
    error_code& err,
    diagnostics& diag
)
{
    // Send request and read the first response
    start_execution_impl(channel, req, output, err, diag);
    if (err)
        return;

    // Read rows and anything else
    while (!output.is_complete())
    {
        if (output.is_reading_head())
        {
            read_resultset_head_impl(channel, output, err, diag);
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

void boost::mysql::detail::async_execute_impl(
    channel& chan,
    const any_execution_request& req,
    execution_processor& output,
    diagnostics& diag,
    boost::asio::any_completion_handler<void(error_code)> handler
)
{
    return boost::asio::async_compose<
        boost::asio::any_completion_handler<void(error_code)>,
        void(boost::mysql::error_code)>(execute_impl_op(chan, req, output, diag), handler, chan);
}

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_QUERY_HPP_ */
