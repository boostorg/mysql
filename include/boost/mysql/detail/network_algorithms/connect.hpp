//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_CONNECT_HPP_
#define INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_CONNECT_HPP_

#include "boost/mysql/detail/network_algorithms/common.hpp"
#include "boost/mysql/connection_params.hpp"

namespace boost {
namespace mysql {
namespace detail {

template <typename StreamType, typename EndpointType>
void connect(
    channel<StreamType>& chan,
    const EndpointType& endpoint,
    const connection_params& params,
    error_code& err,
    error_info& info
);

using connect_signature = empty_signature;

template <typename StreamType, typename EndpointType, typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, connect_signature)
async_connect(
    channel<StreamType>& chan,
    const EndpointType& endpoint,
    const connection_params& params,
    CompletionToken&& token,
    error_info* info
);

} // detail
} // mysql
} // boost

#include "boost/mysql/detail/network_algorithms/impl/connect.hpp"

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_CONNECT_HPP_ */
