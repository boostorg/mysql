//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_COMMON_TEST_CONNECTION_HPP
#define BOOST_MYSQL_TEST_COMMON_TEST_CONNECTION_HPP

#include <boost/mysql/connection.hpp>
#include <boost/mysql/resultset.hpp>
#include <boost/mysql/statement.hpp>

#include <cstddef>

#include "test_stream.hpp"

namespace boost {
namespace mysql {
namespace test {

using test_connection = connection<test_stream>;
using test_statement = statement<test_stream>;
using test_resultset = resultset<test_stream>;

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
