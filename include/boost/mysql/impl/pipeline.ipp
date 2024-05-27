//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_PIPELINE_IPP
#define BOOST_MYSQL_IMPL_PIPELINE_IPP

#pragma once

#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/pipeline.hpp>
#include <boost/mysql/detail/resultset_encoding.hpp>

#include <boost/mysql/impl/internal/protocol/compose_set_names.hpp>
#include <boost/mysql/impl/internal/protocol/serialization.hpp>

#include <boost/throw_exception.hpp>

#include <cstdint>
#include <stdexcept>
#include <vector>

boost::mysql::detail::pipeline_request_stage boost::mysql::detail::serialize_query(
    std::vector<std::uint8_t>& buffer,
    string_view sql
)
{
    return {
        pipeline_stage_kind::execute,
        detail::serialize_top_level(detail::query_command{sql}, buffer),
        resultset_encoding::text
    };
}

boost::mysql::detail::pipeline_request_stage boost::mysql::detail::serialize_execute_statement(
    std::vector<std::uint8_t>& buffer,
    statement stmt,
    span<const field_view> params
)
{
    if (params.size() != stmt.num_params())
    {
        BOOST_THROW_EXCEPTION(
            std::invalid_argument("Wrong number of actual parameters supplied to a prepared statement")
        );
    }
    return {
        pipeline_stage_kind::execute,
        detail::serialize_top_level(detail::execute_stmt_command{stmt.id(), params}, buffer),
        resultset_encoding::binary
    };
}

boost::mysql::detail::pipeline_request_stage boost::mysql::detail::serialize_prepare_statement(
    std::vector<std::uint8_t>& buffer,
    string_view stmt_sql
)
{
    return {
        pipeline_stage_kind::prepare_statement,
        detail::serialize_top_level(detail::prepare_stmt_command{stmt_sql}, buffer),
        {}
    };
}

boost::mysql::detail::pipeline_request_stage boost::mysql::detail::serialize_close_statement(
    std::vector<std::uint8_t>& buffer,
    std::uint32_t stmt_id
)
{
    return {
        pipeline_stage_kind::close_statement,
        detail::serialize_top_level(detail::close_stmt_command{stmt_id}, buffer),
        {}
    };
}

boost::mysql::detail::pipeline_request_stage boost::mysql::detail::serialize_set_character_set(
    std::vector<std::uint8_t>& buffer,
    character_set charset
)
{
    auto q = detail::compose_set_names(charset);
    if (q.has_error())
    {
        BOOST_THROW_EXCEPTION(std::invalid_argument("Invalid character set name"));
    }
    return {
        pipeline_stage_kind::set_character_set,
        detail::serialize_top_level(detail::query_command{*q}, buffer),
        charset
    };
}

boost::mysql::detail::pipeline_request_stage boost::mysql::detail::serialize_reset_connection(
    std::vector<std::uint8_t>& buffer
)
{
    return {
        pipeline_stage_kind::reset_connection,
        detail::serialize_top_level(detail::reset_connection_command{}, buffer),
        {}
    };
}

#endif
