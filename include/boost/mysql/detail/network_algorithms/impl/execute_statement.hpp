//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_STATEMENT_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_STATEMENT_HPP

#pragma once

#include <boost/mysql/detail/auxiliar/stringize.hpp>
#include <boost/mysql/detail/network_algorithms/execute_generic.hpp>
#include <boost/mysql/detail/network_algorithms/execute_statement.hpp>
#include <boost/mysql/detail/protocol/prepared_statement_messages.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>
#include <boost/mysql/error.hpp>
#include <boost/mysql/resultset_base.hpp>
#include <boost/mysql/statement_base.hpp>

#include <boost/asio/compose.hpp>
#include <boost/asio/post.hpp>

namespace boost {
namespace mysql {
namespace detail {

template <class FieldViewFwdIterator>
com_stmt_execute_packet<FieldViewFwdIterator> make_stmt_execute_packet(
    const statement_base& stmt,
    FieldViewFwdIterator params_first,
    FieldViewFwdIterator params_last
)
{
    return com_stmt_execute_packet<FieldViewFwdIterator>{
        stmt.id(),
        std::uint8_t(0),   // flags
        std::uint32_t(1),  // iteration count
        std::uint8_t(1),   // new params flag: set
        params_first,
        params_last,
    };
}

template <class FieldViewFwdIterator>
error_code check_num_params(
    const statement_base& stmt,
    FieldViewFwdIterator params_first,
    FieldViewFwdIterator params_last,
    error_info& info
)
{
    auto param_count = std::distance(params_first, params_last);
    if (param_count != stmt.num_params())
    {
        info.set_message(detail::stringize(
            "execute statement: expected ",
            stmt.num_params(),
            " params, but got ",
            param_count
        ));
        return make_error_code(errc::wrong_num_params);
    }
    else
    {
        return error_code();
    }
}

struct fast_fail_op : boost::asio::coroutine
{
    error_code err_;

    fast_fail_op(error_code err) noexcept : err_(err) {}

    template <class Self>
    void operator()(Self& self)
    {
        BOOST_ASIO_CORO_REENTER(*this)
        {
            BOOST_ASIO_CORO_YIELD boost::asio::post(std::move(self));
            self.complete(err_);
        }
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

template <class Stream, class FieldViewFwdIterator>
void boost::mysql::detail::execute_statement(
    channel<Stream>& chan,
    const statement_base& stmt,
    FieldViewFwdIterator params_first,
    FieldViewFwdIterator params_last,
    const execute_options&,
    resultset_base& output,
    error_code& err,
    error_info& info
)
{
    err = check_num_params(stmt, params_first, params_last, info);
    if (!err)
    {
        execute_generic(
            resultset_encoding::binary,
            chan,
            make_stmt_execute_packet(stmt, params_first, params_last),
            output,
            err,
            info
        );
    }
}

template <class Stream, class FieldViewFwdIterator, class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::detail::async_execute_statement(
    channel<Stream>& chan,
    const statement_base& stmt,
    FieldViewFwdIterator params_first,
    FieldViewFwdIterator params_last,
    const execute_options&,
    resultset_base& output,
    error_info& info,
    CompletionToken&& token
)
{
    error_code err = check_num_params(stmt, params_first, params_last, info);
    if (err)
    {
        return boost::asio::async_compose<CompletionToken, void(error_code)>(
            fast_fail_op(err),
            token,
            chan
        );
    }
    return async_execute_generic(
        resultset_encoding::binary,
        chan,
        make_stmt_execute_packet(stmt, params_first, params_last),
        output,
        info,
        std::forward<CompletionToken>(token)
    );
}

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_STATEMENT_HPP_ */
