//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_PIPELINE_IPP
#define BOOST_MYSQL_IMPL_PIPELINE_IPP

#pragma once

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/pipeline.hpp>
#include <boost/mysql/detail/resultset_encoding.hpp>

#include <boost/mysql/impl/internal/protocol/compose_set_names.hpp>
#include <boost/mysql/impl/internal/protocol/serialization.hpp>

#include <boost/system/system_error.hpp>
#include <boost/throw_exception.hpp>

#include <cstdint>
#include <vector>

boost::mysql::detail::pipeline_request_step boost::mysql::detail::serialize_query(
    std::vector<std::uint8_t>& buffer,
    string_view sql
)
{
    return {
        pipeline_step_kind::execute,
        detail::serialize_top_level(detail::query_command{sql}, buffer, 0),
        resultset_encoding::text
    };
}

boost::mysql::detail::pipeline_request_step boost::mysql::detail::serialize_execute_statement(
    std::vector<std::uint8_t>& buffer,
    statement stmt,
    span<const field_view> params
)
{
    if (params.size() != stmt.num_params())
    {
        BOOST_THROW_EXCEPTION(system::system_error(client_errc::wrong_num_params));
    }
    return {
        pipeline_step_kind::execute,
        detail::serialize_top_level(detail::execute_stmt_command{stmt.id(), params}, buffer, 0),
        resultset_encoding::binary
    };
}

boost::mysql::detail::pipeline_request_step boost::mysql::detail::serialize_prepare_statement(
    std::vector<std::uint8_t>& buffer,
    string_view stmt_sql
)
{
    return {
        pipeline_step_kind::prepare_statement,
        detail::serialize_top_level(detail::prepare_stmt_command{stmt_sql}, buffer, 0),
        {}
    };
}

boost::mysql::detail::pipeline_request_step boost::mysql::detail::serialize_close_statement(
    std::vector<std::uint8_t>& buffer,
    std::uint32_t stmt_id
)
{
    return {
        pipeline_step_kind::close_statement,
        detail::serialize_top_level(detail::close_stmt_command{stmt_id}, buffer, 0),
        {}
    };
}

boost::mysql::detail::pipeline_request_step boost::mysql::detail::serialize_set_character_set(
    std::vector<std::uint8_t>& buffer,
    character_set charset
)
{
    auto q = detail::compose_set_names(charset);
    if (q.has_error())
    {
        BOOST_THROW_EXCEPTION(system::system_error(q.error(), "Invalid charactet set name"));
    }
    return {
        pipeline_step_kind::set_character_set,
        detail::serialize_top_level(detail::query_command{*q}, buffer, 0),
        charset
    };
}

boost::mysql::detail::pipeline_request_step boost::mysql::detail::serialize_reset_connection(
    std::vector<std::uint8_t>& buffer
)
{
    return {
        pipeline_step_kind::reset_connection,
        detail::serialize_top_level(detail::reset_connection_command{}, buffer, 0),
        {}
    };
}

#endif
