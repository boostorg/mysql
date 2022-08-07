//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_HANDSHAKE_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_HANDSHAKE_HPP

#include <boost/mysql/error.hpp>
#include <boost/mysql/detail/channel/channel.hpp>
#include <boost/mysql/connection_params.hpp>


namespace boost {
namespace mysql {
namespace detail {

template <class Stream>
void handshake(
    channel<Stream>& channel,
    const connection_params& params,
    error_code& err,
    error_info& info
);

template <class Stream, class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
async_handshake(
    channel<Stream>& channel,
    const connection_params& params,
    error_info& info,
    CompletionToken&& token
);

} // detail
} // mysql
} // boost

#include <boost/mysql/detail/network_algorithms/impl/handshake.hpp>


#endif
