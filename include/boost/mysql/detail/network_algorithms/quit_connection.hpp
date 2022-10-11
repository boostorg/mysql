//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_QUIT_CONNECTION_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_QUIT_CONNECTION_HPP

#include <boost/mysql/detail/channel/channel.hpp>
#include <boost/mysql/error.hpp>

namespace boost {
namespace mysql {
namespace detail {

template <class Stream>
void quit_connection(channel<Stream>& chan, error_code& code, error_info& info);

template <class Stream, class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
async_quit_connection(channel<Stream>& chan, CompletionToken&& token, error_info& info);

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#include <boost/mysql/detail/network_algorithms/impl/quit_connection.hpp>

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_QUIT_CONNECTION_HPP_ */
