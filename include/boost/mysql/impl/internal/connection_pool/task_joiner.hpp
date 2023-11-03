//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_CONNECTION_POOL_TASK_JOINER_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_CONNECTION_POOL_TASK_JOINER_HPP

#include <boost/mysql/error_code.hpp>

#include <boost/asio/experimental/channel.hpp>

#include <cstddef>

namespace boost {
namespace mysql {
namespace detail {

class wait_group
{
    std::size_t running_tasks_{};
    asio::experimental::channel<void(error_code)> finished_chan_;

public:
    wait_group(boost::asio::any_io_executor ex) : finished_chan_(std::move(ex), 1) {}

    void on_task_start() noexcept { ++running_tasks_; }

    void on_task_finish() noexcept
    {
        if (--running_tasks_ == 0u)
            finished_chan_.try_send(error_code());  // If this happens to fail, terminate() is the best option
    }

    template <class CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    join_tasks(CompletionToken&& token)
    {
        return finished_chan_.async_receive(std::forward<CompletionToken>(token));
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
