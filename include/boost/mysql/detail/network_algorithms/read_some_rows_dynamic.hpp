//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_READ_SOME_ROWS_DYNAMIC_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_READ_SOME_ROWS_DYNAMIC_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/rows_view.hpp>

#include <boost/mysql/detail/channel/channel.hpp>
#include <boost/mysql/detail/execution_processor/execution_state_impl.hpp>

namespace boost {
namespace mysql {
namespace detail {

template <class Stream>
rows_view read_some_rows_dynamic(
    channel<Stream>& chan,
    execution_state_impl& st,
    error_code& err,
    diagnostics& diag
);

template <
    class Stream,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::rows_view))
        CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, rows_view))
async_read_some_rows_dynamic(
    channel<Stream>& chan,
    execution_state_impl& st,
    diagnostics& diag,
    CompletionToken&& token
);

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#include <boost/mysql/detail/network_algorithms/impl/read_some_rows_dynamic.hpp>

#endif
