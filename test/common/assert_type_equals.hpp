//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_COMMON_ASSERT_TYPE_EQUALS_HPP
#define BOOST_MYSQL_TEST_COMMON_ASSERT_TYPE_EQUALS_HPP

#include <type_traits>
#include <boost/type_index.hpp>
#include <boost/test/unit_test.hpp>

namespace boost {
namespace mysql {
namespace test {

template <typename T, typename Expected>
void assert_type_equals()
{
    BOOST_TEST(
        (std::is_same<T, Expected>::value), 
        "Types are different: expected " + boost::typeindex::type_id<Expected>().pretty_name() + ", got " +
        boost::typeindex::type_id<T>().pretty_name());
}

}
}
}

#endif