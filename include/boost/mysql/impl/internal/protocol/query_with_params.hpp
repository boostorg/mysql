//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_QUERY_WITH_PARAMS_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_QUERY_WITH_PARAMS_HPP

#include <boost/mysql/character_set.hpp>
#include <boost/mysql/constant_string_view.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/format_sql.hpp>

#include <boost/mysql/detail/output_string.hpp>

#include <boost/mysql/impl/internal/protocol/impl/serialization_context.hpp>

#include <boost/core/span.hpp>

namespace boost {
namespace mysql {
namespace detail {

struct query_with_params
{
    constant_string_view query;
    span<const format_arg> args;
    format_options opts;

    void serialize(serialization_context& ctx) const
    {
        // Create a format context
        auto fmt_ctx = access::construct<format_context_base>(output_string_ref::create(ctx), opts);

        // Serialize the query header
        ctx.add(0x03);

        // Serialize the actual query
        vformat_sql_to(fmt_ctx, query, args);

        // Check for errors
        ctx.add_error(fmt_ctx.error_state());
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
