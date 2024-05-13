//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_PIPELINE_IPP
#define BOOST_MYSQL_IMPL_PIPELINE_IPP

#pragma once

#include <boost/mysql/metadata_mode.hpp>
#include <boost/mysql/pipeline.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/statement.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/resultset_encoding.hpp>

#include <boost/mysql/impl/internal/protocol/compose_set_names.hpp>
#include <boost/mysql/impl/internal/protocol/serialization.hpp>

#include <cstdint>

void boost::mysql::pipeline::add_query_step(string_view sql, metadata_mode meta)
{
    std::uint8_t seqnum = detail::serialize_top_level(detail::query_command{sql}, impl_.buffer_, 0);
    impl_.steps_.emplace_back(detail::resultset_encoding::text, meta, seqnum);
}

void boost::mysql::pipeline::add_statement_execute_step_range(
    statement stmt,
    span<const field_view> params,
    metadata_mode meta
)
{
    std::uint8_t seqnum = detail::serialize_top_level(
        detail::execute_stmt_command{stmt.id(), params},
        impl_.buffer_,
        0
    );
    impl_.steps_.emplace_back(detail::resultset_encoding::binary, meta, seqnum);
}

void boost::mysql::pipeline::add_prepare_statement(string_view stmt_sql)
{
    std::uint8_t seqnum = detail::serialize_top_level(
        detail::prepare_stmt_command{stmt_sql},
        impl_.buffer_,
        0
    );
    impl_.steps_.emplace_back(pipeline_step_kind::prepare_statement, seqnum);
}

void boost::mysql::pipeline::add_close_statement(statement stmt)
{
    std::uint8_t seqnum = detail::serialize_top_level(
        detail::close_stmt_command{stmt.id()},
        impl_.buffer_,
        0
    );
    impl_.steps_.emplace_back(pipeline_step_kind::close_statement, seqnum);
}

void boost::mysql::pipeline::add_set_character_set(character_set charset)
{
    auto q = detail::compose_set_names(charset);
    // TODO: handle errors properly
    std::uint8_t seqnum = detail::serialize_top_level(detail::query_command{q.value()}, impl_.buffer_, 0);
    impl_.steps_.emplace_back(charset, seqnum);
}

void boost::mysql::pipeline::add_reset_connection()
{
    std::uint8_t seqnum = detail::serialize_top_level(detail::reset_connection_command{}, impl_.buffer_, 0);
    impl_.steps_.emplace_back(pipeline_step_kind::reset_connection, seqnum);
}

void boost::mysql::pipeline::add_ping()
{
    std::uint8_t seqnum = detail::serialize_top_level(detail::ping_command{}, impl_.buffer_, 0);
    impl_.steps_.emplace_back(pipeline_step_kind::ping, seqnum);
}

#endif
