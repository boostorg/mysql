//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_INT_GENERATOR_HPP
#define BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_INT_GENERATOR_HPP

#include <random>

namespace boost {
namespace mysql {
namespace test {

// A wrapper around std::random to generate integers using a uniform distribution
template <class IntType>
class int_generator
{
    std::mt19937 mt_{std::random_device{}()};
    std::uniform_int_distribution<IntType> dist_;

public:
    int_generator(IntType a, IntType b) : dist_(a, b) {}
    IntType generate() { return dist_(mt_); }
};

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
