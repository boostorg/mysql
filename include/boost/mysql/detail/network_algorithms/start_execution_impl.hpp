//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_START_EXECUTION_IMPL_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_START_EXECUTION_IMPL_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>

#include <boost/asio/any_completion_handler.hpp>

namespace boost {
namespace mysql {
namespace detail {

// The caller function must serialize the execution request into channel's buffer
// before calling these

class channel;

BOOST_MYSQL_DECL
void start_execution_impl(
    channel& channel,
    resultset_encoding encoding,
    execution_processor& proc,
    error_code& err,
    diagnostics& diag
);

BOOST_MYSQL_DECL void async_start_execution_impl(
    channel& chan,
    resultset_encoding encoding,
    execution_processor& proc,
    diagnostics& diag,
    asio::any_completion_handler<void(error_code)> handler
);

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#ifdef BOOST_MYSQL_SOURCE
#include <boost/mysql/detail/network_algorithms/impl/start_execution_impl.hpp>
#endif

#endif /* INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_READ_RESULTSET_HEAD_HPP_ */
