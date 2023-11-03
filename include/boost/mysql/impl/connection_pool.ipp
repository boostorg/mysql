//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_CONNECTION_POOL_IPP
#define BOOST_MYSQL_IMPL_CONNECTION_POOL_IPP

#pragma once

#include <boost/mysql/connection_pool.hpp>
#include <boost/mysql/pooled_connection.hpp>

#include <boost/mysql/detail/connection_pool/connection_node.hpp>

namespace boost {
namespace mysql {
namespace detail {

inline owning_pool_params create_pool_params(const pool_params& input)
{
    return {
        any_address(input.server_address),
        input.hparams.username(),
        input.hparams.password(),
        input.hparams.database(),
        input.hparams.connection_collation(),
        input.hparams.ssl(),
        input.hparams.multi_queries(),
        input.initial_size,
        input.max_size,
        input.connect_timeout,
        input.ping_timeout,
        input.reset_timeout,
        input.retry_interval,
        input.ping_interval,
    };
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

void boost::mysql::detail::return_connection(connection_node& node, bool should_reset) noexcept
{
    node.mark_as_collectable(should_reset);
}

const boost::mysql::any_connection* boost::mysql::pooled_connection::const_ptr() const noexcept
{
    return &impl_->connection();
}

boost::mysql::detail::next_connection_action boost::mysql::detail::sansio_connection_node::resume(
    error_code ec,
    collection_state col_st
)
{
    BOOST_ASIO_CORO_REENTER(coro_)
    {
        while (true)
        {
            if (status_ == connection_status::pending_connect)
            {
                // Try to connect
                BOOST_ASIO_CORO_YIELD return next_connection_action::connect;
                if (ec)
                {
                    // We were cancelled, exit
                    if (ec == asio::error::operation_aborted)
                        return next_connection_action::none;

                    // Sleep
                    BOOST_ASIO_CORO_YIELD return next_connection_action::sleep_connect_failed;

                    // We were cancelled, exit
                    if (ec == asio::error::operation_aborted)
                        return next_connection_action::none;

                    // We're still pending connect, just retry. No need to close here.
                }
                else
                {
                    // We're iddle
                    status_ = connection_status::iddle;
                }
            }
            else if (status_ == connection_status::iddle || status_ == connection_status::in_use)
            {
                // Iddle wait. Note that, if a connection is taken, status_ will be
                // changed externally, not by this coroutine. This saves rescheduling.
                BOOST_ASIO_CORO_YIELD return next_connection_action::iddle_wait;
                if (ec == asio::error::operation_aborted)
                {
                    // We were cancelled. Return
                    return next_connection_action::none;
                }
                else if (col_st != collection_state::none)
                {
                    // The user has notified us to collect the connection.
                    // This happens after they return the connection to the pool.
                    // Update status and continue
                    status_ = col_st == collection_state::needs_collect ? connection_status::iddle
                                                                        : connection_status::pending_reset;
                }
                else if (status_ == connection_status::iddle)
                {
                    // The wait finished with no interruptions, and the connection
                    // is still iddle. Time to ping.
                    status_ = connection_status::pending_ping;
                }

                // Otherwise (status is in_use and there's no collection request),
                // the user is still using the connection (it's taking long, but can happen).
                // Iddle wait again until they return the connection.
            }
            else if (status_ == connection_status::pending_ping || status_ == connection_status::pending_reset)
            {
                // Do ping or reset
                BOOST_ASIO_CORO_YIELD return status_ == connection_status::pending_ping
                    ? next_connection_action::ping
                    : next_connection_action::reset;

                // Check result
                if (ec)
                {
                    // The operation was cancelled. Exit
                    if (ec == asio::error::operation_aborted)
                        return next_connection_action::none;

                    // The operation had an error but weren't cancelled. Close and reconnect
                    BOOST_ASIO_CORO_YIELD return next_connection_action::close;
                    status_ = connection_status::pending_connect;
                }
                else
                {
                    // The operation succeeded. We're iddle again.
                    status_ = connection_status::iddle;
                }
            }
            else
            {
                BOOST_ASSERT(false);
            }
        }
    }

    return next_connection_action::none;
}

boost::mysql::connection_pool::connection_pool(asio::any_io_executor ex, const pool_params& params)
    : impl_(new detail::connection_pool_impl(
          detail::create_pool_params(params),
          std::move(ex),
          params.enable_thread_safety
      ))
{
}

#endif
