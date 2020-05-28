//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_EXECUTE_GENERIC_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_EXECUTE_GENERIC_HPP

#include "boost/mysql/detail/network_algorithms/common.hpp"
#include "boost/mysql/resultset.hpp"
#include <string_view>

namespace boost {
namespace mysql {
namespace detail {

template <typename StreamType, typename Serializable>
void execute_generic(
    deserialize_row_fn deserializer,
    channel<StreamType>& channel,
    const Serializable& request,
    resultset<StreamType>& output,
    error_code& err,
    error_info& info
);

template <typename StreamType, typename Serializable, typename CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, resultset<StreamType>))
async_execute_generic(
    deserialize_row_fn deserializer,
    channel<StreamType>& chan,
    const Serializable& request,
    CompletionToken&& token,
    error_info& info
);

} // detail
} // mysql
} // boost

#include "boost/mysql/detail/network_algorithms/impl/execute_generic.hpp"

#endif /* INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_READ_RESULTSET_HEAD_HPP_ */
