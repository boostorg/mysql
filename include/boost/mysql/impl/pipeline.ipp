//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_PIPELINE_IPP
#define BOOST_MYSQL_IMPL_PIPELINE_IPP

#pragma once

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/pipeline.hpp>

#include <boost/mysql/impl/internal/protocol/compose_set_names.hpp>
#include <boost/mysql/impl/internal/protocol/serialization.hpp>

#include <cstdint>
#include <vector>

std::uint8_t boost::mysql::detail::serialize_query(std::vector<std::uint8_t>& buffer, string_view sql)
{
    return detail::serialize_top_level(detail::query_command{sql}, buffer, 0);
}

std::uint8_t boost::mysql::detail::serialize_execute_statement(
    std::vector<std::uint8_t>& buffer,
    statement stmt,
    span<const field_view> params
)
{
    return detail::serialize_top_level(detail::execute_stmt_command{stmt.id(), params}, buffer, 0);
}

std::uint8_t boost::mysql::detail::serialize_prepare_statement(
    std::vector<std::uint8_t>& buffer,
    string_view stmt_sql
)
{
    return detail::serialize_top_level(detail::prepare_stmt_command{stmt_sql}, buffer, 0);
}

std::uint8_t boost::mysql::detail::serialize_close_statement(
    std::vector<std::uint8_t>& buffer,
    statement stmt
)
{
    return detail::serialize_top_level(detail::close_stmt_command{stmt.id()}, buffer, 0);
}

std::uint8_t boost::mysql::detail::serialize_set_character_set(
    std::vector<std::uint8_t>& buffer,
    character_set charset
)
{
    auto q = detail::compose_set_names(charset);
    // TODO: handle errors properly
    return detail::serialize_top_level(detail::query_command{q.value()}, buffer, 0);
}

std::uint8_t boost::mysql::detail::serialize_reset_connection(std::vector<std::uint8_t>& buffer)
{
    return detail::serialize_top_level(detail::reset_connection_command{}, buffer, 0);
}

std::uint8_t boost::mysql::detail::serialize_ping(std::vector<std::uint8_t>& buffer)
{
    return detail::serialize_top_level(detail::ping_command{}, buffer, 0);
}

#endif
