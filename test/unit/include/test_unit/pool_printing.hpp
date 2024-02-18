//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_POOL_PRINTING_HPP
#define BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_POOL_PRINTING_HPP

#include <boost/mysql/impl/internal/connection_pool/sansio_connection_node.hpp>

#include <ostream>

namespace boost {
namespace mysql {
namespace detail {

inline std::ostream& operator<<(std::ostream& os, connection_status v)
{
    switch (v)
    {
    case connection_status::initial: return os << "connection_status::initial";
    case connection_status::connect_in_progress: return os << "connection_status::connect_in_progress";
    case connection_status::sleep_connect_failed_in_progress:
        return os << "connection_status::sleep_connect_failed_in_progress";
    case connection_status::reset_in_progress: return os << "connection_status::reset_in_progress";
    case connection_status::ping_in_progress: return os << "connection_status::ping_in_progress";
    case connection_status::idle: return os << "connection_status::idle";
    case connection_status::in_use: return os << "connection_status::in_use";
    default: return os << "<unknown connection_status>";
    }
}

inline std::ostream& operator<<(std::ostream& os, collection_state v)
{
    switch (v)
    {
    case collection_state::needs_collect: return os << "collection_state::needs_collect";
    case collection_state::needs_collect_with_reset:
        return os << "collection_state::needs_collect_with_reset";
    case collection_state::none: return os << "collection_state::none";
    default: return os << "<unknown collection_state>";
    }
}

inline std::ostream& operator<<(std::ostream& os, next_connection_action v)
{
    switch (v)
    {
    case next_connection_action::none: return os << "next_connection_action::none";
    case next_connection_action::connect: return os << "next_connection_action::connect";
    case next_connection_action::sleep_connect_failed:
        return os << "next_connection_action::sleep_connect_failed";
    case next_connection_action::idle_wait: return os << "next_connection_action::idle_wait";
    case next_connection_action::reset: return os << "next_connection_action::reset";
    case next_connection_action::ping: return os << "next_connection_action::ping";
    default: return os << "<unknown next_connection_action>";
    }
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
