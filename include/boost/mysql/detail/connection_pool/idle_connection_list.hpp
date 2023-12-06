//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_CONNECTION_POOL_IDLE_CONNECTION_LIST_HPP
#define BOOST_MYSQL_DETAIL_CONNECTION_POOL_IDLE_CONNECTION_LIST_HPP

#include <boost/mysql/error_code.hpp>

#include <boost/asio/error.hpp>
#include <boost/intrusive/list.hpp>

namespace boost {
namespace mysql {
namespace detail {

// TODO: we may want to get rid of intrusive to decrease build times
using hook_type = intrusive::list_base_hook<>;

class connection_node;

class idle_connection_list
{
    intrusive::list<connection_node, intrusive::base_hook<hook_type>> list_;

public:
    idle_connection_list() = default;

    connection_node* try_get_one() noexcept { return list_.empty() ? nullptr : &list_.back(); }

    void add_one(connection_node& node) { list_.push_back(node); }

    void remove(connection_node& node) { list_.erase(list_.iterator_to(node)); }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
