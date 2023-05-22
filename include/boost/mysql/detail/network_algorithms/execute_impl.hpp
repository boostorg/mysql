//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_EXECUTE_IMPL_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_EXECUTE_IMPL_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/channel/channel.hpp>
#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>

#include <boost/asio/async_result.hpp>

namespace boost {
namespace mysql {
namespace detail {

// The caller function must serialize the execution request into channel's buffer
// before calling these

template <class Stream>
void execute_impl(
    channel<Stream>& channel,
    resultset_encoding enc,
    execution_processor& output,
    error_code& err,
    diagnostics& diag
);

template <class Stream, BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
async_execute_impl(
    channel<Stream>& chan,
    resultset_encoding enc,
    execution_processor& output,
    diagnostics& diag,
    CompletionToken&& token
);

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#include <boost/mysql/detail/network_algorithms/impl/execute_impl.hpp>

#endif
