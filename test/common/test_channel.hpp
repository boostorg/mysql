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

using test_channel = detail::channel<test_stream>;

inline test_channel create_channel(std::vector<std::uint8_t> messages = {})
{
    return test_channel (0, std::move(messages));
}

} // test
} // mysql
} // boost

#endif
