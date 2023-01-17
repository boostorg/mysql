//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_READ_ONE_ROW_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_READ_ONE_ROW_HPP

#include <boost/mysql/error_code.hpp>
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/row.hpp>
#include <boost/mysql/server_diagnostics.hpp>

#include <boost/mysql/detail/channel/channel.hpp>

namespace boost {
namespace mysql {
namespace detail {

template <class Stream>
row_view read_one_row(
    channel<Stream>& channel,
    execution_state& st,
    error_code& err,
    server_diagnostics& diag
);

template <
    class Stream,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::row_view))
        CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, row_view))
async_read_one_row(
    channel<Stream>& channel,
    execution_state& st,
    server_diagnostics& diag,
    CompletionToken&& token
);

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#include <boost/mysql/detail/network_algorithms/impl/read_one_row.hpp>

#endif /* INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_READ_TEXT_ROW_HPP_ */
