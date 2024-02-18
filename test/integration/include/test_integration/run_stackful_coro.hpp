//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_INTEGRATION_INCLUDE_TEST_INTEGRATION_RUN_STACKFUL_CORO_HPP
#define BOOST_MYSQL_TEST_INTEGRATION_INCLUDE_TEST_INTEGRATION_RUN_STACKFUL_CORO_HPP

#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>

#include <functional>

namespace boost {
namespace mysql {
namespace test {

inline void run_stackful_coro(
    boost::asio::io_context& ctx,
    std::function<void(boost::asio::yield_context)> fn
)
{
    boost::asio::spawn(ctx.get_executor(), fn, [](std::exception_ptr err) {
        if (err)
            std::rethrow_exception(err);
    });
    ctx.run();
}

inline void run_stackful_coro(std::function<void(boost::asio::yield_context)> fn)
{
    boost::asio::io_context ctx;
    run_stackful_coro(ctx, std::move(fn));
}

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
