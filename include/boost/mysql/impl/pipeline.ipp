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
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/pipeline.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/pipeline.hpp>
#include <boost/mysql/detail/resultset_encoding.hpp>

#include <boost/mysql/impl/internal/protocol/compose_set_names.hpp>
#include <boost/mysql/impl/internal/protocol/serialization.hpp>

#include <boost/core/span.hpp>
#include <boost/throw_exception.hpp>

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace boost {
namespace mysql {
namespace detail {

inline std::uint8_t serialize_execute_statement(
    statement stmt,
    span<const field_view> params,
    std::vector<std::uint8_t>& buff
)
{
    if (params.size() != stmt.num_params())
    {
        BOOST_THROW_EXCEPTION(
            std::invalid_argument("Wrong number of actual parameters supplied to a prepared statement")
        );
    }
    return serialize_top_level(execute_stmt_command{stmt.id(), params}, buff);
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

// TODO: refactor this
boost::mysql::detail::pipeline_request_stage boost::mysql::execute_stage::create(
    std::vector<std::uint8_t>& buff
) const
{
    std::uint8_t seqnum;
    detail::resultset_encoding enc;

    switch (type_)
    {
    case type_query:
        seqnum = detail::serialize_top_level(detail::query_command{data_.query}, buff);
        enc = detail::resultset_encoding::text;
        break;
    case type_stmt_tuple:
    {
        enc = detail::resultset_encoding::binary;
        constexpr std::size_t stack_fields = 64u;
        auto params = data_.stmt_tuple.params;
        auto stmt = data_.stmt_tuple.stmt;
        if (params.size() <= stack_fields)
        {
            std::array<field_view, stack_fields> storage;
            for (std::size_t i = 0; i < params.size(); ++i)
                storage[i] = params[i].impl_;
            seqnum = detail::serialize_execute_statement(stmt, {storage.data(), params.size()}, buff);
        }
        else
        {
            std::vector<field_view> storage;
            storage.reserve(params.size());
            for (auto p : params)
                storage.push_back(p.impl_);
            seqnum = detail::serialize_execute_statement(stmt, storage, buff);
        }
        break;
    }
    case type_stmt_range:
        seqnum = detail::serialize_execute_statement(data_.stmt_range.stmt, data_.stmt_range.params, buff);
        enc = detail::resultset_encoding::binary;
        break;
    }

    return {detail::pipeline_stage_kind::execute, seqnum, enc};
}

boost::mysql::detail::pipeline_request_stage boost::mysql::prepare_statement_stage::create(
    std::vector<std::uint8_t>& buffer
) const
{
    return {
        detail::pipeline_stage_kind::prepare_statement,
        detail::serialize_top_level(detail::prepare_stmt_command{stmt_sql_}, buffer),
        {}
    };
}

boost::mysql::detail::pipeline_request_stage boost::mysql::close_statement_stage::create(
    std::vector<std::uint8_t>& buffer
) const
{
    return {
        detail::pipeline_stage_kind::close_statement,
        detail::serialize_top_level(detail::close_stmt_command{stmt_id_}, buffer),
        {}
    };
}

boost::mysql::detail::pipeline_request_stage boost::mysql::reset_connection_stage::create(
    std::vector<std::uint8_t>& buffer
) const
{
    return {
        detail::pipeline_stage_kind::reset_connection,
        detail::serialize_top_level(detail::reset_connection_command{}, buffer),
        {}
    };
}

boost::mysql::detail::pipeline_request_stage boost::mysql::set_character_set_stage::create(
    std::vector<std::uint8_t>& buffer
) const
{
    auto q = detail::compose_set_names(charset_);
    if (q.has_error())
    {
        BOOST_THROW_EXCEPTION(std::invalid_argument("Invalid character set name"));
    }
    return {
        detail::pipeline_stage_kind::set_character_set,
        detail::serialize_top_level(detail::query_command{*q}, buffer),
        charset_
    };
}

void boost::mysql::any_stage_response::check_has_results() const
{
    if (!has_results())
    {
        BOOST_THROW_EXCEPTION(
            std::invalid_argument("any_stage_response::as_results: object doesn't contain results")
        );
    }
}

boost::mysql::statement boost::mysql::any_stage_response::as_statement() const
{
    if (!has_statement())
    {
        BOOST_THROW_EXCEPTION(
            std::invalid_argument("any_stage_response::as_statement: object doesn't contain a statement")
        );
    }
    return variant2::unsafe_get<1>(impl_);
}

#endif
