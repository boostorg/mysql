//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_COMMON_INCLUDE_TEST_COMMON_CONTEXT_UTILS_HPP
#define BOOST_MYSQL_TEST_COMMON_INCLUDE_TEST_COMMON_CONTEXT_UTILS_HPP

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/io_context.hpp>

#include <functional>

#include "test_common/source_location.hpp"

namespace boost {
namespace mysql {
namespace test {

// TODO: split
// TODO: remove the executor overloads
// poll until *done == true
void poll_until(asio::io_context& ctx, const bool* done, source_location loc = BOOST_MYSQL_CURRENT_LOCATION);
void poll_until(
    asio::any_io_executor ex,
    const bool* done,
    source_location loc = BOOST_MYSQL_CURRENT_LOCATION
);

// poll until done() == true
void poll_until(
    asio::io_context& ctx,
    const std::function<bool()>& done,
    source_location loc = BOOST_MYSQL_CURRENT_LOCATION
);
void poll_until(
    asio::any_io_executor ex,
    const std::function<bool()>& done,
    source_location loc = BOOST_MYSQL_CURRENT_LOCATION
);

struct io_context_fixture
{
    asio::io_context ctx;

    // Checks that we effectively run out of work
    ~io_context_fixture();
};

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
