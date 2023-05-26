//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_HIGH_LEVEL_EXECUTION_IPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_HIGH_LEVEL_EXECUTION_IPP

#pragma once

#include <boost/mysql/detail/channel/channel.hpp>
#include <boost/mysql/detail/network_algorithms/high_level_execution.hpp>
#include <boost/mysql/detail/protocol/prepared_statement_messages.hpp>
#include <boost/mysql/detail/protocol/protocol_types.hpp>
#include <boost/mysql/detail/protocol/query_messages.hpp>
#include <boost/mysql/detail/protocol/serialization.hpp>

void boost::mysql::detail::serialize_execution_request(string_view sql, channel& chan)
{
    com_query_packet request{string_eof(sql)};
    serialize_message(request, chan.current_capabilities(), chan.shared_buffer());
}

void boost::mysql::detail::serialize_execution_request_impl(
    std::uint32_t stmt_id,
    span<const field_view> params,
    channel& chan
)
{
    com_stmt_execute_packet<const field_view*> request{
        stmt_id,
        std::uint8_t(0),   // flags
        std::uint32_t(1),  // iteration count
        std::uint8_t(1),   // new params flag: set
        params.begin(),
        params.end(),
    };
    serialize_message(request, chan.current_capabilities(), chan.shared_buffer());
}

#endif
