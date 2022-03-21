//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_HANDSHAKE_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_HANDSHAKE_HPP

#include <boost/utility/string_view.hpp>
#include <boost/mysql/detail/network_algorithms/common.hpp>
#include <boost/mysql/detail/protocol/channel.hpp>
#include <boost/mysql/detail/protocol/protocol_types.hpp>
#include <boost/mysql/connection_params.hpp>
#include <boost/mysql/collation.hpp>

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
    CompletionToken&& token,
    error_info& info
);

} // detail
} // mysql
} // boost

#include <boost/mysql/detail/network_algorithms/impl/handshake.hpp>


#endif
