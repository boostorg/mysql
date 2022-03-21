//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_EXECUTE_GENERIC_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_EXECUTE_GENERIC_HPP

#include <boost/mysql/detail/network_algorithms/common.hpp>
#include <boost/mysql/resultset.hpp>
#include <boost/utility/string_view.hpp>

namespace boost {
namespace mysql {
namespace detail {

template <class Stream, class Serializable>
void execute_generic(
    deserialize_row_fn deserializer,
    channel<Stream>& channel,
    const Serializable& request,
    resultset<Stream>& output,
    error_code& err,
    error_info& info
);

template <class Stream, class Serializable, class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, resultset<Stream>))
async_execute_generic(
    deserialize_row_fn deserializer,
    channel<Stream>& chan,
    const Serializable& request,
    CompletionToken&& token,
    error_info& info
);

} // detail
} // mysql
} // boost

#include <boost/mysql/detail/network_algorithms/impl/execute_generic.hpp>

#endif /* INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_READ_RESULTSET_HEAD_HPP_ */
