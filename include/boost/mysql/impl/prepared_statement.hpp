//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_PREPARED_STATEMENT_HPP
#define BOOST_MYSQL_IMPL_PREPARED_STATEMENT_HPP

#include "boost/mysql/detail/network_algorithms/execute_statement.hpp"
#include "boost/mysql/detail/network_algorithms/close_statement.hpp"
#include "boost/mysql/detail/auxiliar/stringize.hpp"
#include <boost/asio/bind_executor.hpp>

template <typename Stream>
template <typename ForwardIterator>
void boost::mysql::prepared_statement<Stream>::check_num_params(
    ForwardIterator first,
    ForwardIterator last,
    error_code& err,
    error_info& info
) const
{
    auto param_count = std::distance(first, last);
    if (param_count != num_params())
    {
        err = detail::make_error_code(errc::wrong_num_params);
        info.set_message(detail::stringize(
                "prepared_statement::execute: expected ", num_params(), " params, but got ", param_count));
    }
}


template <typename Stream>
template <typename ForwardIterator>
boost::mysql::resultset<Stream> boost::mysql::prepared_statement<Stream>::execute(
    ForwardIterator params_first,
    ForwardIterator params_last,
    error_code& err,
    error_info& info
)
{
    assert(valid());

    mysql::resultset<Stream> res;
    detail::clear_errors(err, info);

    // Verify we got passed the right number of params
    check_num_params(params_first, params_last, err, info);
    if (!err)
    {
        detail::execute_statement(
            *channel_,
            stmt_msg_.statement_id.value,
            params_first,
            params_last,
            res,
            err,
            info
        );
    }

    return res;
}

template <typename Stream>
template <typename ForwardIterator>
boost::mysql::resultset<Stream> boost::mysql::prepared_statement<Stream>::execute(
    ForwardIterator params_first,
    ForwardIterator params_last
)
{
    detail::error_block blk;
    auto res = execute(params_first, params_last, blk.err, blk.info);
    blk.check();
    return res;
}

template <typename StreamType>
template <typename ForwardIterator, BOOST_ASIO_COMPLETION_TOKEN_FOR(
    void(boost::mysql::error_code, boost::mysql::resultset<StreamType>)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code, boost::mysql::resultset<StreamType>)
)
boost::mysql::prepared_statement<StreamType>::async_execute(
    ForwardIterator params_first,
    ForwardIterator params_last,
    error_info& output_info,
    CompletionToken&& token
)
{
    output_info.clear();
    assert(valid());

    // Check we got passed the right number of params
    error_code err;
    check_num_params(params_first, params_last, err, output_info);

    auto initiation = [](auto&& handler, error_code err, error_info& info,
            prepared_statement<StreamType>& stmt, ForwardIterator params_first,
            ForwardIterator params_last) {
        if (err)
        {
            auto executor = boost::asio::get_associated_executor(
                handler,
                stmt.next_layer().get_executor()
            );

            boost::asio::post(boost::asio::bind_executor(
                executor,
                [handler = std::forward<decltype(handler)>(handler), err] () mutable {
                    std::forward<decltype(handler)>(handler)(err, resultset<StreamType>());
                }
            ));
        }
        else
        {
            // Actually execute the statement
            detail::async_execute_statement(
                *stmt.channel_,
                stmt.stmt_msg_.statement_id.value,
                params_first,
                params_last,
                std::forward<decltype(handler)>(handler),
                info
            );
        }
    };

    return boost::asio::async_initiate<CompletionToken, void(error_code, resultset<StreamType>)>(
        initiation,
        token,
        err,
        std::ref(output_info),
        std::ref(*this),
        params_first,
        params_last
    );
}

template <typename StreamType>
void boost::mysql::prepared_statement<StreamType>::close(
    error_code& code,
    error_info& info
)
{
    assert(valid());
    detail::clear_errors(code, info);
    detail::close_statement(*channel_, id(), code, info);
}

template <typename StreamType>
void boost::mysql::prepared_statement<StreamType>::close()
{
    assert(valid());
    detail::error_block blk;
    detail::close_statement(*channel_, id(), blk.err, blk.info);
    blk.check();
}

template <typename StreamType>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code)
)
boost::mysql::prepared_statement<StreamType>::async_close(
    error_info& output_info,
    CompletionToken&& token
)
{
    assert(valid());
    output_info.clear();
    return detail::async_close_statement(
        *channel_,
        id(),
        std::forward<CompletionToken>(token),
        output_info
    );
}

#endif /* INCLUDE_BOOST_MYSQL_IMPL_PREPARED_STATEMENT_HPP_ */
