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

#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/execution_processor/execution_state_impl.hpp>

#include <boost/asio/any_completion_handler.hpp>

namespace boost {
namespace mysql {
namespace detail {

class channel;

BOOST_MYSQL_DECL
rows_view read_some_rows_dynamic_impl(
    channel& chan,
    execution_state_impl& st,
    error_code& err,
    diagnostics& diag
);

BOOST_MYSQL_DECL void async_read_some_rows_dynamic_impl(
    channel& chan,
    execution_state_impl& st,
    diagnostics& diag,
    asio::any_completion_handler<void(error_code, rows_view)> handler
);

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#ifdef BOOST_MYSQL_SOURCE
#include <boost/mysql/detail/network_algorithms/impl/read_some_rows_dynamic.hpp>
#endif

#endif
