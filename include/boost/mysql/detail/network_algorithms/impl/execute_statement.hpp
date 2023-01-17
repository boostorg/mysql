//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_STATEMENT_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_STATEMENT_HPP

#pragma once

#include <boost/mysql/resultset.hpp>
#include <boost/mysql/statement_base.hpp>

#include <boost/mysql/detail/auxiliar/field_type_traits.hpp>
#include <boost/mysql/detail/network_algorithms/execute_statement.hpp>
#include <boost/mysql/detail/network_algorithms/read_all_rows.hpp>
#include <boost/mysql/detail/network_algorithms/start_statement_execution.hpp>

#include <type_traits>

namespace boost {
namespace mysql {
namespace detail {

template <class Stream, BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple>
struct execute_statement_op : boost::asio::coroutine
{
    channel<Stream>& chan_;
    server_diagnostics& diag_;
    const statement_base& stmt_;
    FieldLikeTuple params_;
    resultset& output_;

    // We need a deduced context to enable perfect forwarding
    template <BOOST_MYSQL_FIELD_LIKE_TUPLE DeducedTuple>
    execute_statement_op(
        channel<Stream>& chan,
        server_diagnostics& diag,
        const statement_base& stmt,
        DeducedTuple&& params,
        resultset& output
    ) noexcept
        : chan_(chan),
          diag_(diag),
          stmt_(stmt),
          params_(std::forward<DeducedTuple>(params)),
          output_(output)
    {
    }

    template <class Self>
    void operator()(Self& self, error_code err = {})
    {
        // Error checking
        if (err)
        {
            self.complete(err);
            return;
        }

        // Normal path
        BOOST_ASIO_CORO_REENTER(*this)
        {
            BOOST_ASIO_CORO_YIELD
            async_start_statement_execution(
                chan_,
                stmt_,
                std::move(params_),
                output_.state(),
                diag_,
                std::move(self)
            );

            BOOST_ASIO_CORO_YIELD async_read_all_rows(
                chan_,
                output_.state(),
                output_.mutable_rows(),
                diag_,
                std::move(self)
            );

            self.complete(error_code());
        }
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

template <class Stream, BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple>
void boost::mysql::detail::execute_statement(
    channel<Stream>& channel,
    const statement_base& stmt,
    const FieldLikeTuple& params,
    resultset& output,
    error_code& err,
    server_diagnostics& diag
)
{
    start_statement_execution(channel, stmt, params, output.state(), err, diag);
    if (err)
        return;

    read_all_rows(channel, output.state(), output.mutable_rows(), err, diag);
}

template <
    class Stream,
    BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::detail::async_execute_statement(
    channel<Stream>& chan,
    const statement_base& stmt,
    FieldLikeTuple&& params,
    resultset& output,
    server_diagnostics& diag,
    CompletionToken&& token
)
{
    using decayed_tuple = typename std::decay<FieldLikeTuple>::type;
    return boost::asio::async_compose<CompletionToken, void(boost::mysql::error_code)>(
        execute_statement_op<Stream, decayed_tuple>(
            chan,
            diag,
            stmt,
            std::forward<FieldLikeTuple>(params),
            output
        ),
        token,
        chan
    );
}

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_QUERY_HPP_ */
