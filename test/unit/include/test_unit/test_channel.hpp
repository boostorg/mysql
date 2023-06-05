//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_TEST_CHANNEL_HPP
#define BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_TEST_CHANNEL_HPP

#include <boost/mysql/error_code.hpp>
#include <boost/mysql/throw_on_error.hpp>

#include <boost/mysql/detail/channel/channel.hpp>

#include <cstddef>

#include "test_stream.hpp"

namespace boost {
namespace mysql {
namespace test {

using test_channel = detail::channel<test_stream>;

inline test_channel create_channel(std::vector<std::uint8_t> messages = {}, std::size_t buffer_size = 1024)
{
    return test_channel(buffer_size, std::move(messages));
}

inline void read_some_and_check(test_channel& chan)
{
    error_code err;
    chan.read_some(err);
    throw_on_error(err);
}

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
