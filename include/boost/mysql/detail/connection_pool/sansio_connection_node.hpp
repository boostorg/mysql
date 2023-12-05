//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_CONNECTION_POOL_SANSIO_CONNECTION_NODE_HPP
#define BOOST_MYSQL_DETAIL_CONNECTION_POOL_SANSIO_CONNECTION_NODE_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/config.hpp>

#include <boost/asio/coroutine.hpp>
#include <boost/assert.hpp>

namespace boost {
namespace mysql {
namespace detail {

// The status the connection is in
enum class connection_status
{
    // Connection task hasn't initiated yet.
    // This status doesn't count as pending. This facilitates tracking pending connections.
    initial,

    // Connection is trying to connect
    pending_connect,

    // Connection is trying to reset
    pending_reset,

    // Connection is trying to ping
    pending_ping,

    // Connection can be handed to the user
    idle,

    // Connection has been handed to the user
    in_use,

    // Connection has been terminated.
    // This status doesn't count as pending. This facilitates tracking pending connections.
    terminated,
};

// The next I/O action the connection should take. There's
// no 1-1 mapping to connection_status
enum class next_connection_action
{
    // Do nothing, exit the loop
    none,

    // Issue a connect
    connect,

    // Connect failed, issue a sleep
    sleep_connect_failed,

    // Wait until a collection request is issued or the ping interval elapses
    idle_wait,

    // Issue a reset
    reset,

    // Issue a ping
    ping,
};

// A collection_state represents the possibility that a connection
// that was in_use was returned by the user
enum class collection_state
{
    // Connection wasn't returned
    none,

    // Connection was returned and needs reset
    needs_collect,

    // Connection was returned and doesn't need reset
    needs_collect_with_reset
};

inline bool is_pending(connection_status status) noexcept
{
    return status == connection_status::pending_connect || status == connection_status::pending_reset ||
           status == connection_status::pending_ping;
}

// CRTP. Derived should implement the entering_xxx and exiting_xxx hook functions.
// Derived must derive from this class
template <class Derived>
class sansio_connection_node
{
    asio::coroutine coro_;
    connection_status status_{connection_status::initial};

    void set_status(connection_status new_status)
    {
        auto& derived = static_cast<Derived&>(*this);

        // Notify we're entering/leaving the idle status
        if (new_status == connection_status::idle && status_ != connection_status::idle)
            derived.entering_idle();
        else if (new_status != connection_status::idle && status_ == connection_status::idle)
            derived.exiting_idle();

        // Notify we're entering/leaving a pending status
        if (!is_pending(status_) && is_pending(new_status))
            derived.entering_pending();
        else if (is_pending(status_) && !is_pending(new_status))
            derived.exiting_pending();

        // Actually update status
        status_ = new_status;
    }

public:
    void mark_as_in_use() noexcept
    {
        BOOST_ASSERT(status_ == connection_status::idle);
        set_status(connection_status::in_use);
    }

    void cancel() noexcept { set_status(connection_status::terminated); }

    next_connection_action resume(error_code ec, collection_state col_st)
    {
        BOOST_ASIO_CORO_REENTER(coro_)
        {
            while (true)
            {
                if (status_ == connection_status::initial)
                {
                    set_status(connection_status::pending_connect);
                }
                else if (status_ == connection_status::pending_connect)
                {
                    // Try to connect
                    BOOST_ASIO_CORO_YIELD return next_connection_action::connect;
                    if (ec)
                    {
                        // Sleep
                        BOOST_ASIO_CORO_YIELD return next_connection_action::sleep_connect_failed;

                        // We're still pending connect, just retry.
                    }
                    else
                    {
                        // We're idle
                        set_status(connection_status::idle);
                    }
                }
                else if (status_ == connection_status::idle || status_ == connection_status::in_use)
                {
                    // Idle wait. Note that, if a connection is taken, status_ will be
                    // changed externally, not by this coroutine. This saves rescheduling.
                    BOOST_ASIO_CORO_YIELD return next_connection_action::idle_wait;
                    if (col_st != collection_state::none)
                    {
                        // The user has notified us to collect the connection.
                        // This happens after they return the connection to the pool.
                        // Update status and continue
                        set_status(
                            col_st == collection_state::needs_collect ? connection_status::pending_reset
                                                                      : connection_status::idle
                        );
                    }
                    else if (status_ == connection_status::idle)
                    {
                        // The wait finished with no interruptions, and the connection
                        // is still idle. Time to ping.
                        set_status(connection_status::pending_ping);
                    }

                    // Otherwise (status is in_use and there's no collection request),
                    // the user is still using the connection (it's taking long, but can happen).
                    // Idle wait again until they return the connection.
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
                        // The operation had an error but weren't cancelled. Reconnect
                        set_status(connection_status::pending_connect);
                    }
                    else
                    {
                        // The operation succeeded. We're idle again.
                        set_status(connection_status::idle);
                    }
                }
                else if (status_ == connection_status::terminated)
                {
                    return next_connection_action::none;
                }
                else
                {
                    BOOST_ASSERT(false);
                }
            }
        }

        return next_connection_action::none;
    }

    // Exposed for testing
    connection_status status() const noexcept { return status_; }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
