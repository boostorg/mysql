//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/any_address.hpp>
#include <boost/mysql/character_set.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>

#include <boost/mysql/detail/next_action.hpp>
#include <boost/mysql/detail/pipeline.hpp>
#include <boost/mysql/detail/results_iterator.hpp>
#include <boost/mysql/detail/resultset_encoding.hpp>

#include <boost/mysql/impl/internal/connection_pool/sansio_connection_node.hpp>
#include <boost/mysql/impl/internal/protocol/capabilities.hpp>
#include <boost/mysql/impl/internal/protocol/db_flavor.hpp>

#include <ostream>

#include "test_common/printing.hpp"
#include "test_unit/printing.hpp"

using namespace boost::mysql;

// address_type
static const char* to_string(address_type v)
{
    switch (v)
    {
    case address_type::host_and_port: return "address_type::host_and_port";
    case address_type::unix_path: return "address_type::unix_path";
    default: return "<unknown address_type>";
    }
}

std::ostream& boost::mysql::operator<<(std::ostream& os, address_type v) { return os << ::to_string(v); }

// capabilities
std::ostream& boost::mysql::detail::operator<<(std::ostream& os, const capabilities& v)
{
    return os << "capabilities{" << v.get() << "}";
}

// db_flavor
static const char* to_string(detail::db_flavor v)
{
    switch (v)
    {
    case detail::db_flavor::mysql: return "db_flavor::mysql";
    case detail::db_flavor::mariadb: return "db_flavor::mariadb";
    default: return "<unknown db_flavor>";
    }
}

std::ostream& boost::mysql::detail::operator<<(std::ostream& os, db_flavor v) { return os << ::to_string(v); }

// resultset_encoding
static const char* to_string(detail::resultset_encoding v)
{
    switch (v)
    {
    case detail::resultset_encoding::text: return "resultset_encoding::text";
    case detail::resultset_encoding::binary: return "resultset_encoding::binary";
    default: return "<unknown resultset_encoding>";
    }
}

std::ostream& boost::mysql::detail::operator<<(std::ostream& os, detail::resultset_encoding v)
{
    return os << ::to_string(v);
}

// results_iterator
std::ostream& boost::mysql::detail::operator<<(std::ostream& os, const results_iterator& it)
{
    return os << "results_iterator{ .self = " << static_cast<const void*>(it.obj())
              << ", .index = " << it.index() << "}";
}

// next_action_type;
static const char* to_string(detail::next_action_type v)
{
    switch (v)
    {
    case detail::next_action_type::none: return "next_action_type::none";
    case detail::next_action_type::read: return "next_action_type::read";
    case detail::next_action_type::write: return "next_action_type::write";
    case detail::next_action_type::ssl_handshake: return "next_action_type::ssl_handshake";
    case detail::next_action_type::ssl_shutdown: return "next_action_type::ssh_shutdown";
    default: return "<unknown next_action_type>";
    }
}

std::ostream& boost::mysql::detail::operator<<(std::ostream& os, next_action_type v)
{
    return os << ::to_string(v);
}

// pipeline_stage_kind
static const char* to_string(detail::pipeline_stage_kind v)
{
    switch (v)
    {
    case detail::pipeline_stage_kind::execute: return "pipeline_stage_kind::execute";
    case detail::pipeline_stage_kind::prepare_statement: return "pipeline_stage_kind::prepare_statement";
    case detail::pipeline_stage_kind::close_statement: return "pipeline_stage_kind::close_statement";
    case detail::pipeline_stage_kind::reset_connection: return "pipeline_stage_kind::reset_connection";
    case detail::pipeline_stage_kind::set_character_set: return "pipeline_stage_kind::set_character_set";
    case detail::pipeline_stage_kind::ping: return "pipeline_stage_kind::ping";
    default: return "<unknown pipeline_stage_kind>";
    }
}

std::ostream& boost::mysql::detail::operator<<(std::ostream& os, pipeline_stage_kind v)
{
    return os << ::to_string(v);
}

// pipeline_request_stage
bool boost::mysql::detail::operator==(const pipeline_request_stage& lhs, const pipeline_request_stage& rhs)
{
    if (lhs.kind != rhs.kind || lhs.seqnum != rhs.seqnum)
        return false;
    switch (lhs.kind)
    {
    case pipeline_stage_kind::execute: return lhs.stage_specific.enc == rhs.stage_specific.enc;
    case pipeline_stage_kind::set_character_set:
        return lhs.stage_specific.charset == rhs.stage_specific.charset;
    default: return true;
    }
}

std::ostream& boost::mysql::detail::operator<<(std::ostream& os, pipeline_request_stage v)
{
    os << "pipeline_request_stage{ .kind = " << v.kind << ", .seqnum = " << +v.seqnum;
    switch (v.kind)
    {
    case pipeline_stage_kind::execute: os << ", .enc = " << v.stage_specific.enc; break;
    case pipeline_stage_kind::set_character_set: os << ", .charset = " << v.stage_specific.charset; break;
    default: break;
    }
    return os << " }";
}

// connection_status
static const char* to_string(detail::connection_status v)
{
    switch (v)
    {
    case detail::connection_status::initial: return "connection_status::initial";
    case detail::connection_status::connect_in_progress: return "connection_status::connect_in_progress";
    case detail::connection_status::sleep_connect_failed_in_progress:
        return "connection_status::sleep_connect_failed_in_progress";
    case detail::connection_status::reset_in_progress: return "connection_status::reset_in_progress";
    case detail::connection_status::ping_in_progress: return "connection_status::ping_in_progress";
    case detail::connection_status::idle: return "connection_status::idle";
    case detail::connection_status::in_use: return "connection_status::in_use";
    default: return "<unknown connection_status>";
    }
}

std::ostream& boost::mysql::detail::operator<<(std::ostream& os, connection_status v)
{
    return os << ::to_string(v);
}

// collection_state
static const char* to_string(detail::collection_state v)
{
    switch (v)
    {
    case detail::collection_state::needs_collect: return "collection_state::needs_collect";
    case detail::collection_state::needs_collect_with_reset:
        return "collection_state::needs_collect_with_reset";
    case detail::collection_state::none: return "collection_state::none";
    default: return "<unknown collection_state>";
    }
}

std::ostream& boost::mysql::detail::operator<<(std::ostream& os, collection_state v)
{
    return os << ::to_string(v);
}

static const char* to_string(detail::next_connection_action v)
{
    switch (v)
    {
    case detail::next_connection_action::none: return "next_connection_action::none";
    case detail::next_connection_action::connect: return "next_connection_action::connect";
    case detail::next_connection_action::sleep_connect_failed:
        return "next_connection_action::sleep_connect_failed";
    case detail::next_connection_action::idle_wait: return "next_connection_action::idle_wait";
    case detail::next_connection_action::reset: return "next_connection_action::reset";
    case detail::next_connection_action::ping: return "next_connection_action::ping";
    default: return "<unknown next_connection_action>";
    }
}

std::ostream& boost::mysql::detail::operator<<(std::ostream& os, next_connection_action v)
{
    return os << ::to_string(v);
}
