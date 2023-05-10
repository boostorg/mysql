//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_COMMON_CREATION_CREATE_STATIC_EXECUTION_STATE_HPP
#define BOOST_MYSQL_TEST_COMMON_CREATION_CREATE_STATIC_EXECUTION_STATE_HPP

#include <boost/mysql/detail/execution_processor/static_execution_state_impl.hpp>

#include "creation/create_execution_processor.hpp"

namespace boost {
namespace mysql {
namespace test {

template <class... StaticRow>
using static_exec_builder = basic_exec_builder<detail::static_execution_state_impl<StaticRow...>>;

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
