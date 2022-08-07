//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PROTOCOL_DESERIALIZE_ROW_HPP
#define BOOST_MYSQL_DETAIL_PROTOCOL_DESERIALIZE_ROW_HPP

#include <boost/asio/buffer.hpp>
#include <boost/mysql/detail/protocol/capabilities.hpp>
#include <boost/mysql/detail/protocol/serialization.hpp>
#include <boost/mysql/detail/protocol/text_deserialization.hpp>
#include <boost/mysql/detail/protocol/binary_deserialization.hpp>
#include <boost/mysql/resultset.hpp>
#include <boost/mysql/error.hpp>
#include <boost/mysql/value.hpp>
#include <vector>


namespace boost {
namespace mysql {
namespace detail {


inline bool deserialize_row(
    boost::asio::const_buffer read_message,
    capabilities current_capabilities,
    resultset& resultset,
	std::vector<value>& output,
    error_code& err,
    error_info& info
)
{
    assert(resultset.valid());

    // Message type: row, error or eof?
    std::uint8_t msg_type = 0;
    deserialization_context ctx (read_message, current_capabilities);
    err = make_error_code(deserialize(ctx, msg_type));
    if (err)
        return false;
    if (msg_type == eof_packet_header)
    {
        // end of resultset => this is a ok_packet, not a row
        ok_packet ok_pack;
        err = deserialize_message(ctx, ok_pack);
        if (err)
            return false;
        resultset.complete(ok_pack);
        output.clear();
        return false;
    }
    else if (msg_type == error_packet_header)
    {
        // An error occurred during the generation of the rows
        err = process_error_packet(ctx, info);
        return false;
    }
    else
    {
        // An actual row
        ctx.rewind(1); // keep the 'message type' byte, as it is part of the actual message
        err = resultset.encoding() == detail::resultset_encoding::text ?
                deserialize_text_row(ctx, resultset.meta(), output) :
                deserialize_binary_row(ctx, resultset.meta(), output);
        if (err)
            return false;
        return true;
    }
}


} // detail
} // mysql
} // boost


#endif
