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
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/statement.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/execution_processor/execution_processor.hpp>

#include <boost/asio/any_completion_handler.hpp>
#include <boost/core/span.hpp>

namespace boost {
namespace mysql {
namespace detail {

struct any_execution_request
{
    union data_t
    {
        string_view query;
        struct
        {
            statement stmt;
            span<const field_view> params;
        } stmt;

        data_t(string_view q) noexcept : query(q) {}
        data_t(statement s, span<const field_view> params) noexcept : stmt{s, params} {}
    } data;
    bool is_query;

    any_execution_request(string_view q) noexcept : data(q), is_query(true) {}
    any_execution_request(statement s, span<const field_view> params) noexcept
        : data(s, params), is_query(false)
    {
    }
};

class channel;

BOOST_MYSQL_DECL
void start_execution_impl(
    channel& channel,
    const any_execution_request& req,
    execution_processor& proc,
    error_code& err,
    diagnostics& diag
);

BOOST_MYSQL_DECL void async_start_execution_impl(
    channel& chan,
    const any_execution_request& req,
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
