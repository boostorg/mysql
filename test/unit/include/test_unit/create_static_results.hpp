//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_CREATE_STATIC_RESULTS_HPP
#define BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_CREATE_STATIC_RESULTS_HPP

#include <boost/mysql/detail/execution_processor/static_results_impl.hpp>

#include "creation/create_execution_processor.hpp"

namespace boost {
namespace mysql {
namespace test {

template <class... StaticRow>
using static_results_builder = basic_exec_builder<detail::static_results_impl<StaticRow...>>;

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
