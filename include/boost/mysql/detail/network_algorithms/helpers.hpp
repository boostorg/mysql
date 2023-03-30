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
#include <boost/mysql/detail/protocol/deserialize_row.hpp>
#include <boost/mysql/detail/protocol/execution_state_impl.hpp>

namespace boost {
namespace mysql {
namespace detail {

inline void process_available_rows(
    channel_base& channel,
    execution_state_impl& st,
    error_code& err,
    diagnostics& diag
)
{
    // Process all read messages until they run out, an error happens
    // or an EOF is received
    st.on_row_batch_start();
    while (channel.has_read_messages() && st.should_read_rows())
    {
        // Get the row message
        auto message = channel.next_read_message(st.sequence_number(), err);
        if (err)
            return;

        // Deserialize the row
        deserialize_row(message, channel.current_capabilities(), channel.flavor(), st, err, diag);
        if (err)
            return;
    }
    st.on_row_batch_finish();
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
