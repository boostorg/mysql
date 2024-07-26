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

#include <cstdint>

namespace boost {
namespace mysql {
namespace detail {

class external_format_context : public format_context_base
{
    static void do_append(void* obj, const char* data, std::size_t size)
    {
        static_cast<serialization_context*>(obj)->add({reinterpret_cast<const std::uint8_t*>(data), size});
    }

public:
    external_format_context(serialization_context& ctx, format_options opts) noexcept
        : format_context_base(output_string_ref(&do_append, &ctx), opts)
    {
    }
};

struct query_with_params
{
    constant_string_view query;
    span<const format_arg> args;
    format_options opts;

    void serialize(serialization_context& ctx) const
    {
        // Create a format context
        external_format_context fmt_ctx(ctx, opts);

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
