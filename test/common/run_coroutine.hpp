//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_COMMON_RUN_COROUTINE_HPP
#define BOOST_MYSQL_TEST_COMMON_RUN_COROUTINE_HPP

#include <boost/asio/use_awaitable.hpp>

#ifdef BOOST_ASIO_HAS_CO_AWAIT

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>

namespace boost {
namespace mysql {
namespace test {

inline void run_coroutine(std::function<boost::asio::awaitable<void>(void)> coro)
{
    boost::asio::io_context ctx;
    auto fut = boost::asio::co_spawn(ctx.get_executor(), std::move(coro), boost::asio::use_future);
    ctx.run();
    fut.get();
}
}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif  // BOOST_ASIO_HAS_CO_AWAIT

#endif
