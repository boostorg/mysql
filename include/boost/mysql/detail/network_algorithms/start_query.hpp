//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_START_QUERY_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_START_QUERY_HPP

#include <boost/mysql/error.hpp>
#include <boost/mysql/execution_state.hpp>

#include <boost/mysql/detail/channel/channel.hpp>

#include <boost/asio/async_result.hpp>
#include <boost/utility/string_view.hpp>

namespace boost {
namespace mysql {
namespace detail {

template <class Stream>
void start_query(
    channel<Stream>& channel,
    boost::string_view query,
    execution_state& output,
    error_code& err,
    error_info& info
);

template <
    class Stream,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
async_start_query(
    channel<Stream>& chan,
    boost::string_view query,
    execution_state& output,
    error_info& info,
    CompletionToken&& token
);

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#include <boost/mysql/detail/network_algorithms/impl/start_query.hpp>

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_EXECUTE_QUERY_HPP_ */
