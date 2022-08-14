//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_READ_ALL_ROWS_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_READ_ALL_ROWS_HPP

#include <boost/mysql/error.hpp>
#include <boost/mysql/detail/channel/channel.hpp>
#include <boost/mysql/resultset.hpp>
#include <boost/mysql/rows.hpp>


namespace boost {
namespace mysql {
namespace detail {

template <class Stream>
void read_all_rows(
    channel<Stream>& channel,
    resultset& resultset,
	rows& output,
    error_code& err,
    error_info& info
);

template <class Stream, class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
async_read_all_rows(
    channel<Stream>& channel,
    resultset& resultset,
	rows& output,
    error_info& output_info,
    CompletionToken&& token
);


} // detail
} // mysql
} // boost

#include <boost/mysql/detail/network_algorithms/impl/read_all_rows.hpp>



#endif /* INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_READ_TEXT_ROW_HPP_ */
