//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_CONNECTION_POOL_WAIT_GROUP_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_CONNECTION_POOL_WAIT_GROUP_HPP

#include <boost/mysql/error_code.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/basic_waitable_timer.hpp>

#include <cstddef>

namespace boost {
namespace mysql {
namespace detail {

template <class ClockType>
class wait_group
{
    std::size_t running_tasks_{};
    asio::basic_waitable_timer<ClockType> finished_;

public:
    wait_group(asio::any_io_executor ex) : finished_(std::move(ex), (ClockType::time_point::max)()) {}

    asio::any_io_executor get_executor() { return finished_.get_executor(); }

    void on_task_start() { ++running_tasks_; }

    void on_task_finish()
    {
        if (--running_tasks_ == 0u)
            finished_.expires_at((ClockType::time_point::min)());
    }

    // Note: this operation always completes with a cancelled error code
    // (for simplicity).
    template <class CompletionToken>
    void async_wait(CompletionToken&& token)
    {
        finished_.async_wait(std::forward<CompletionToken>(token));
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
