//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_CONNECTION_POOL_TIMER_LIST_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_CONNECTION_POOL_TIMER_LIST_HPP

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/basic_waitable_timer.hpp>

#include <utility>

namespace boost {
namespace mysql {
namespace detail {

// TODO: do we want to keep this file?
// TODO: rename
template <class ClockType>
class timer_list
{
    asio::basic_waitable_timer<ClockType> timer_;

public:
    timer_list(asio::any_io_executor ex) : timer_(ex, (ClockType::time_point::max)()) {}

    // TODO: c++11
    template <class CompletionToken>
    auto async_wait(CompletionToken&& token)
    {
        return timer_.async_wait(std::forward<CompletionToken>(token));
    }

    void notify_one() { timer_.cancel_one(); }

    void notify_all() { timer_.expires_at((ClockType::time_point::min)()); }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
