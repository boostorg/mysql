//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_QUIT_CONNECTION_HPP_
#define INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_QUIT_CONNECTION_HPP_

#include "boost/mysql/detail/network_algorithms/common.hpp"

namespace boost {
namespace mysql {
namespace detail {

template <typename StreamType>
void compose_quit(
    channel<StreamType>& chan
)
{
    serialize_message(
        quit_packet(),
        chan.current_capabilities(),
        chan.shared_buffer()
    );
    chan.reset_sequence_number();
}

template <typename StreamType>
void quit_connection(
    channel<StreamType>& chan,
    error_code& code,
    error_info&
)
{
    compose_quit(chan);
    chan.write(boost::asio::buffer(chan.shared_buffer()), code);
}

using quit_connection_signature = empty_signature;

template <typename StreamType, typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, quit_connection_signature)
async_quit_connection(
    channel<StreamType>& chan,
    CompletionToken&& token,
    error_info*
)
{
    compose_quit(chan);
    return chan.async_write(boost::asio::buffer(chan.shared_buffer()), std::forward<CompletionToken>(token));
}

} // detail
} // mysql
} // boost


#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_QUIT_CONNECTION_HPP_ */
