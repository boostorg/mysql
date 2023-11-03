//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_CONNECTION_POOL_IPP
#define BOOST_MYSQL_IMPL_CONNECTION_POOL_IPP

#pragma once

#include <boost/mysql/connection_pool.hpp>
#include <boost/mysql/pooled_connection.hpp>

void boost::mysql::detail::return_connection(connection_node& node, bool should_reset) noexcept
{
    node.mark_as_collectable(should_reset);
}

const boost::mysql::any_connection* boost::mysql::pooled_connection::const_ptr() const noexcept
{
    return &impl_->connection();
}

#endif
