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
#include <boost/system/result.hpp>

#include <cstdint>

namespace boost {
namespace mysql {
namespace detail {

class external_format_context : public format_context_base
{
    static void do_append(void* obj, const char* data, std::size_t size)
    {
        static_cast<serialization_context*>(obj)->add_checked(
            {reinterpret_cast<const std::uint8_t*>(data), size}
        );
    }

public:
    external_format_context(serialization_context& ctx, format_options opts) noexcept
        : format_context_base(output_string_ref(&do_append, &ctx), opts)
    {
    }
};

// TODO: can we make serialization able to fail in the general case?
inline system::result<std::uint8_t> serialize_query_with_params(
    std::vector<std::uint8_t>& to,
    constant_string_view query,
    span<const format_arg> args,
    format_options opts,
    std::size_t frame_size = max_packet_size
)
{
    // Create a serialization context
    serialization_context ctx(to, frame_size);

    // Serialize the query header
    ctx.add(0x03);

    // Create a format context and serialize the actual query
    external_format_context fmt_ctx(ctx, opts);
    vformat_sql_to(fmt_ctx, query, args);

    // Check for errors
    auto err = fmt_ctx.error_state();
    if (err)
        return err;

    // Write frame headers
    return ctx.write_frame_headers(0);
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
