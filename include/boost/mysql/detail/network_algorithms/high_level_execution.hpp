//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_HIGH_LEVEL_EXECUTION_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_HIGH_LEVEL_EXECUTION_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/auxiliar/execution_request.hpp>
#include <boost/mysql/detail/channel/channel.hpp>
#include <boost/mysql/detail/execution_processor/execution_processor.hpp>

#include <boost/asio/async_result.hpp>

namespace boost {
namespace mysql {
namespace detail {

template <class Stream, BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest>
void execute(
    channel<Stream>& channel,
    const ExecutionRequest& req,
    execution_processor& output,
    error_code& err,
    diagnostics& diag
);

template <
    class Stream,
    BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
async_execute(
    channel<Stream>& chan,
    ExecutionRequest&& req,
    execution_processor& output,
    diagnostics& diag,
    CompletionToken&& token
);

template <class Stream, BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest>
void start_execution(
    channel<Stream>& channel,
    const ExecutionRequest& req,
    execution_processor& st,
    error_code& err,
    diagnostics& diag
);

template <
    class Stream,
    BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
async_start_execution(
    channel<Stream>& chan,
    ExecutionRequest&& req,
    execution_processor& st,
    diagnostics& diag,
    CompletionToken&& token
);

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#include <boost/mysql/detail/network_algorithms/impl/high_level_execution.hpp>

#endif
