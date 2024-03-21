//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_INTEGRATION_INCLUDE_TEST_INTEGRATION_SNIPPETS_RUN_CORO_HPP
#define BOOST_MYSQL_TEST_INTEGRATION_INCLUDE_TEST_INTEGRATION_SNIPPETS_RUN_CORO_HPP

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>

#include <functional>

namespace boost {
namespace mysql {
namespace test {

#ifdef BOOST_ASIO_HAS_CO_AWAIT
inline void run_coro(boost::asio::any_io_executor ex, std::function<boost::asio::awaitable<void>(void)> fn)
{
    boost::asio::co_spawn(ex, fn, [](std::exception_ptr ptr) {
        if (ptr)
        {
            std::rethrow_exception(ptr);
        }
    });
    static_cast<boost::asio::io_context&>(ex.context()).run();
}
#endif

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
