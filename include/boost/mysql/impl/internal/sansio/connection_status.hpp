//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_CONNECTION_STATUS_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_CONNECTION_STATUS_HPP

namespace boost {
namespace mysql {
namespace detail {

enum class connection_status
{
    // Never connected, closed or with fatal error
    not_connected,

    // Connected and ready for a command
    ready,

    // In the middle of a multi-function operation
    engaged_in_multi_function,
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
