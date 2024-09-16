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
#include <boost/intrusive/list.hpp>
#include <boost/intrusive/list_hook.hpp>

#include <cstddef>

namespace boost {
namespace mysql {
namespace detail {

template <class ClockType>
struct timer_block : intrusive::list_base_hook<intrusive::link_mode<intrusive::auto_unlink>>
{
    asio::basic_waitable_timer<ClockType> timer;

    timer_block(asio::any_io_executor ex) : timer(std::move(ex)) {}
};

template <class ClockType>
class timer_list
{
    intrusive::list<timer_block<ClockType>, intrusive::constant_time_size<false>> requests_;

public:
    timer_list() = default;
    void push_back(timer_block<ClockType>& req) noexcept { requests_.push_back(req); }
    void notify_one()
    {
        for (auto& req : requests_)
        {
            if (req.timer.cancel())
                return;
        }
    }
    void notify_all()
    {
        for (auto& req : requests_)
            req.timer.cancel();
    }
    std::size_t size() const noexcept { return requests_.size(); }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
