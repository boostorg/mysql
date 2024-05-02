//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_CONSTANTS_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_CONSTANTS_HPP

#include <boost/config.hpp>

#include <cstddef>

namespace boost {
namespace mysql {
namespace detail {

BOOST_INLINE_CONSTEXPR std::size_t max_packet_size = 0xffffff;
BOOST_INLINE_CONSTEXPR std::size_t frame_header_size = 4;

enum class db_flavor
{
    mysql,
    mariadb
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
