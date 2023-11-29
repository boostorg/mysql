//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_CONNECTION_POOL_RUN_WITH_TIMEOUT_HPP
#define BOOST_MYSQL_DETAIL_CONNECTION_POOL_RUN_WITH_TIMEOUT_HPP

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/asio/deferred.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/experimental/cancellation_condition.hpp>
#include <boost/asio/experimental/channel_error.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/steady_timer.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <utility>

namespace boost {
namespace mysql {
namespace detail {

inline error_code to_error_code(
    std::array<std::size_t, 2> completion_order,
    error_code io_ec,
    error_code timer_ec
) noexcept
{
    if (completion_order[0] == 0u)  // I/O finished first
        return io_ec == asio::experimental::error::channel_cancelled ||
                       io_ec == asio::experimental::error::channel_closed
                   ? asio::error::operation_aborted
                   : io_ec;
    else if (completion_order[1] == 0u && !timer_ec)  // Timer fired. Operation timed out
        return client_errc::timeout;
    else  // Timer was cancelled
        return asio::error::operation_aborted;
}

// Timer's expiry should be set by the caller
template <class Op, class Handler>
void run_with_timeout_impl(asio::steady_timer& timer, Op&& op, Handler&& handler)
{
    auto adapter = [](std::array<std::size_t, 2> completion_order, error_code io_ec, error_code timer_ec) {
        return asio::deferred.values(to_error_code(completion_order, io_ec, timer_ec));
    };
    asio::experimental::make_parallel_group(std::forward<Op>(op), timer.async_wait(asio::deferred))
        .async_wait(asio::experimental::wait_for_one(), asio::deferred(adapter))(std::forward<Handler>(handler
        ));
}

template <class Op, class Handler>
void run_with_timeout(
    asio::steady_timer& timer,
    std::chrono::steady_clock::time_point tp,
    Op&& op,
    Handler&& handler
)
{
    timer.expires_at(tp);
    run_with_timeout_impl(timer, std::forward<Op>(op), std::forward<Handler>(handler));
}

template <class Op, class Handler>
void run_with_timeout(
    asio::steady_timer& timer,
    std::chrono::steady_clock::duration dur,
    Op&& op,
    Handler&& handler
)
{
    timer.expires_after(dur);
    run_with_timeout_impl(timer, std::forward<Op>(op), std::forward<Handler>(handler));
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
