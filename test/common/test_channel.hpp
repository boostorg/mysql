//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_COMMON_TEST_CHANNEL_HPP
#define BOOST_MYSQL_TEST_COMMON_TEST_CHANNEL_HPP

#include <boost/mysql/detail/channel/channel.hpp>
#include "test_stream.hpp"

namespace boost {
namespace mysql {
namespace test {

class test_channel : public detail::channel<test_stream>
{
public:
    test_channel() : detail::channel<test_stream>(0) {} // initial buffer empty
};


} // test
} // mysql
} // boost

#endif
