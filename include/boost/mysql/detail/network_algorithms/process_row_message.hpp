//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_HELPERS_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_HELPERS_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/channel/channel.hpp>
#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/protocol/deserialize_execution_messages.hpp>

#include <boost/config.hpp>

namespace boost {
namespace mysql {
namespace detail {

BOOST_ATTRIBUTE_NODISCARD inline error_code process_row_message(
    channel_base& channel,
    execution_processor& proc,
    diagnostics& diag,
    const output_ref& ref
)
{
    // Get the row message
    error_code err;
    auto buff = channel.next_read_message(proc.sequence_number(), err);
    if (err)
        return err;

    // Deserialize it
    auto msg = deserialize_row_message(buff, channel.current_capabilities(), channel.flavor(), diag);
    switch (msg.type)
    {
    case row_message::type_t::error: err = msg.data.err; break;
    case row_message::type_t::ok_packet: err = proc.on_row_ok_packet(msg.data.ok_pack); break;
    case row_message::type_t::row: err = proc.on_row(msg.data.ctx, ref, channel.shared_fields()); break;
    }

    return err;
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
