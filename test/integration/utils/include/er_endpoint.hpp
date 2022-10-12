//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_INTEGRATION_UTILS_INCLUDE_ER_ENDPOINT_HPP
#define BOOST_MYSQL_TEST_INTEGRATION_UTILS_INCLUDE_ER_ENDPOINT_HPP

namespace boost {
namespace mysql {
namespace test {

enum class er_endpoint
{
    valid,
    inexistent
};

}
}  // namespace mysql
}  // namespace boost

#endif