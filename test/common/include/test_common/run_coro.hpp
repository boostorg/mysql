//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_INTEGRATION_INCLUDE_TEST_INTEGRATION_RUN_CORO_HPP
#define BOOST_MYSQL_TEST_INTEGRATION_INCLUDE_TEST_INTEGRATION_RUN_CORO_HPP

#include <boost/assert/source_location.hpp>
#include <boost/capy/task.hpp>

namespace boost {
namespace mysql {
namespace test {

void run_coro(capy::task<void> fn, boost::source_location loc = BOOST_CURRENT_LOCATION);

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
