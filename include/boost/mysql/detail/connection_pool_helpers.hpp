//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_CONNECTION_POOL_HELPERS_HPP
#define BOOST_MYSQL_DETAIL_CONNECTION_POOL_HELPERS_HPP

#include <boost/mysql/detail/config.hpp>

namespace boost {
namespace mysql {
namespace detail {

// Foward decl
class connection_node;
class connection_pool_impl;

BOOST_MYSQL_DECL
void return_connection(connection_node& node, bool should_reset) noexcept;

struct connection_node_deleter
{
    void operator()(detail::connection_node* node) const noexcept { return_connection(*node, true); }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
